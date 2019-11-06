/******************************************************************************
8266 thing connecting through MQTT
******************************************************************************/

// Wifi and MQTT headers
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Headers for persistent memory
#include <EEPROM.h>

WiFiClient wclient;
PubSubClient client(wclient); // Setup MQTT client

/*  Storing all relevent configuration data in non-volatile memory (via EEPROM).  
 *  Creating a structure to hold all that data.
 */
#define SSID_SIZE     40
#define PASSWORD_SIZE 20
typedef struct 
{
  char ssid[SSID_SIZE];
  char password[PASSWORD_SIZE];
  
  // PubSubClient needs broker address as 4 integers seperated by commas 
  // (example:  10,0,0,7 instead of 10.0.0.7)
  //  Thus, I'm gonna store them as 4 bytes.  
  unsigned char broker_addr[4];

  // I'm going to limit client ID to 20 characters
  char client_id[21];
} nv_data_type;

nv_data_type nv_data;

/* state of "how far" we're connected */
typedef enum
{
  STATE_OFFLINE = 0,             // Not looking for network...inputting parameters.
  STATE_DISCONNECT,              // Disconnected, but looking for WiFi
  STATE_LOOKING_FOR_BROKER,      // Connected to WiFi and looking for broker
  STATE_ACTIVE                   // Connected to broker and exchanging MQTT messages.
} state_type;

// Forward function definitions for our state machine
void init_offline_state();
state_type process_offline_state();
void init_disconnect_state();
state_type process_disconnect_state();
void init_looking_for_broker();
state_type process_looking_for_broker();
void init_active();
state_type process_active();

/*==============================================================
 * MQTT Callback function
 *
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
  int i;
  
  Serial.println("MQTT Message received!");
  Serial.print("Topic: ");
  Serial.print(topic);
  Serial.print(", payload: ");
  
  for (i = 0; i < length; i++) 
  {
    Serial.print( (char) payload[i]);
  }

  Serial.println();
  
}
/*=============================================================
 * SERIAL HELPER FUNCITONS
 */

/*===============================================
 * serial_read_number
 * 
 * Reads a number from the serial port.  Returns -1 if invalid.
 * Blocks.  Only allows 3 digit positive numbers.
 */
int serial_read_number( void )
{
  char c;
  int number=-1;

  //Serial.println("serial_read_number");
  
  // keep going until either we get a number (followed by \n), or an invalid character.
  while (true)
  {
    if (Serial.available())
    {
      c = Serial.read();

      //Serial.print("char: ");
      //Serial.println(c);

      if (c == '\n') return number;
      if (c < '0') return -1;
      if (c > '9') return -1;

      // If we get to here, that means the c char is a digit.  Add it in to our number.
      if (number =- -1) number = 0;

      number = number * 10;
      number = number + (c - '0');
    }
  }
  
}

/*===============================================
 * serial_read_string
 * 
 * Reads a string from the serial port.
 */
void serial_read_string( char *str, int max_chars )
{
  char c;
  int char_cnt=0;

  //Serial.println("serial_read_string");
  
  // keep going until either we get a number (followed by \n), or an invalid character.
  while (true)
  {
    if (Serial.available())
    {
      c = Serial.read();

      //Serial.print("char: ");
      //Serial.println(c);

      // if we've got a \n, then we want to properly terminate the string and return.
      if (c == '\n')
      {
        *str = NULL;
        return;
      }
      // otherwise, keep building the string.
      else
      {
        *str = c;
        str++;
        char_cnt++;
        
        if (char_cnt == (max_chars -1))
        {
          *str = NULL;
          return;
        }
      }
    } 
  }
}

/*===============================================
 * print_broker_addr
 * 
 * Prints a line with our broker address out the serial port.
 */
void print_broker_addr( void )
{
  int i;

  for (i=0;i<4;i++)
  {
    Serial.print(nv_data.broker_addr[i]);
    Serial.print(".");
  }
  Serial.println();
}


/*===================================================================
 * OFFLINE STATE FUNCTIONS 
 * =================================================================*/
 
 /*======================================
  * configure_ssid
  * 
  * This function reads a string from the serial port and uses it to 
  * set our SSID.
  */
void configure_ssid( void )
{ 
  Serial.println("Enter SSID");
  
  serial_read_string(nv_data.ssid, SSID_SIZE);

  Serial.print("Set SSID to ");
  Serial.println(nv_data.ssid);
  
  EEPROM.put(0,nv_data);
  EEPROM.commit();

}

 /*======================================
  * configure_password
  * 
  * This function reads a string from the serial port and uses it to 
  * set our WiFi password.
  */
void configure_pasword( void )
{ 
  Serial.println("Enter password");
  
  serial_read_string(nv_data.password, PASSWORD_SIZE);

  Serial.print("Set password");
  
  EEPROM.put(0,nv_data);
  EEPROM.commit();

}

 /*======================================
  * configure_broker
  * 
  * This function reads a string from the serial port and uses it to 
  * set our broker address.  Note that this string needs to be in IPV4 format.
  */
#define BROKER_STR_SIZE 16
void configure_broker( void )
{
  char broker_string[BROKER_STR_SIZE];
  int  addr_int;
  int  addr_index;
  char *str_ptr=broker_string;
  
  Serial.println("Enter broker address (format:  127.0.0.1)");

  //start by reading in the string from the serial port.
  serial_read_string(broker_string, BROKER_STR_SIZE);

  // Now that we've got the string, we need to parse it.  
  // We're expecting IPV4 format (eg "127.0.0.1")
  // We'll use the addr_int variable to accumulate each address byte, and
  // addr_index to tell which byte we're looking at.
  //    Example above:  127 is the 0th addr_index, 1 is the 3rd.
  addr_int = -1;
  addr_index = 0;
  while (true)
  {

    // If we've got a dot, go to the next address index.
    if (*str_ptr == '.')
    {
      // if we haven't gotten a valid number, print an error and exit.
      if ( (addr_int < 0) || (addr_int > 255) )
      {
        Serial.println("Invalid byte in address");
        return;
      }
      else
      {
        //Serial.println("Found a dot...going to next address");
        
        nv_data.broker_addr[addr_index] = addr_int;
        str_ptr++;
        addr_int = -1;

        // make sure we don't have too many dots...
        if (addr_index >= 3)
        {
           Serial.println("Invalid address...more than 4 entries");
           return; 
        }
        else
        {
          addr_index++;
        }
      }
    }
    // if this is the end of our string, make sure we're on the fourth byte, and confirm
    // that it's in range.
    else if (*str_ptr == NULL)
    {
      if ((addr_index == 3) && (addr_int >= 0) && (addr_int <=255))
      {
        //Serial.println("Found End of String.");
        nv_data.broker_addr[addr_index] = addr_int;
        break;
      }
      else
      {
        Serial.println("invalid address");
        return;
      }
    }
    // If we've got a digit, build our current address.
    else if ( (*str_ptr >= '0') && (*str_ptr <= '9') )
    {
      if (addr_int == -1) addr_int = 0;
      addr_int = addr_int * 10;
      addr_int = addr_int + (*str_ptr - '0');      

      // Serial.print("Saw a ");
      // Serial.print(*str_ptr);
      // Serial.print("...number = ");
      // Serial.println(addr_int);
      
      str_ptr++;
    }
    
    // If we got here, this is an invalid character.
    else
    {
      Serial.print("Invalid char in parsing broker address: ");
      Serial.println(*str_ptr);
      return;
    }
  }

  // at this point, we should have built a valid address.  Commit it to NV.
  Serial.print("Broker Address = ");
  Serial.print(nv_data.broker_addr[0]);
  Serial.print(".");
  Serial.print(nv_data.broker_addr[1]);
  Serial.print(".");
  Serial.print(nv_data.broker_addr[2]);
  Serial.print(".");
  Serial.println(nv_data.broker_addr[3]);

  
  EEPROM.put(0,nv_data);
  EEPROM.commit();
  
}

/*======================================
 * configure_client_id
 * 
 * This function reads a string from the serial port and uses it to 
 * set our MQTT client id.
 */
void configure_client_id( void )
{ 
  Serial.println("Enter client ID");
  Serial.println("  (no spaces, 20 chars max)");

  serial_read_string(nv_data.client_id, 21);

  Serial.print("Set client to ");
  Serial.println(nv_data.client_id);
  
  
  EEPROM.put(0,nv_data);
  EEPROM.commit();

}

/*=====================================
 * print_offline_menu
 */
void print_offline_menu( void )
{
  Serial.println("Configuration:");
  Serial.println("1....set WiFi SSID");
  Serial.println("2....set WiFi password");
  Serial.println("3....set MQTT Broker");
  Serial.println("4....set MQTT Client Name");
  Serial.println("5....exit offline mode");
}

void init_offline_state( void )
{
  bool buffer_flushed = false;
  char c;
  
  // To get to offline state, the user needed to type something in the 
  // serial window...which means we have a buffer of chars up to a '/n'.  Flush those.
  while (!buffer_flushed)
  {
    if (Serial.available())
    {
      c = Serial.read();
      if (c == '\n')
      {
        buffer_flushed = true;
      }
    }
  }  
}

/*==================================================
 * PROCESS_OFFLINE_STATE
 * 
 * This is the state handler function for the offline state.
 * In offline state, we allow the user to program (via the serial port)
 * our WiFi and MQTT configuration data. 
 */
state_type process_offline_state( void )
{
  int input;

  print_offline_menu();

  input = serial_read_number();

  //Serial.print("Read a ");
  //Serial.println(input);

    switch (input)
    {
      case 1:
        configure_ssid();
      break;

      case 2:
        configure_pasword();
      break;
      
      case 3:
        configure_broker();
      break;

      case 4:
        configure_client_id();
      break;

      case 5:
        init_disconnect_state();
        return STATE_DISCONNECT;
        
      default:
        Serial.println("Unknown command");
        print_offline_menu();
    }

  return STATE_OFFLINE;
  
}

/*=================================================================
 * DISCONNECT STATE FUNCTIONS
 =================================================================*/

/*=========================================
 * init_disconnect_state
 * 
 * Initializes our disconnected state by starting to look for WiFi
 */
void init_disconnect_state( void )
{
  Serial.print("\nConnecting to network");
  WiFi.begin(nv_data.ssid, nv_data.password); // Connect to network
}

/*=========================================
 * process_disconnect_state
 * 
 * Processes our disconnect state by continously checking our WiFi status.
 * We'll stay in disconnect if we're not connected..otherwise we'll start
 * looking for our broker.
 */
state_type process_disconnect_state( void )
{
    if (WiFi.status() != WL_CONNECTED) 
    { 
      // Wait for connection
      delay(500);
      Serial.print(".");
      return STATE_DISCONNECT;
    }
    else
    {
      init_looking_for_broker();
      return STATE_LOOKING_FOR_BROKER;
    }
}



/*=================================================================
 * LOOKING_FOR_BROKER STATE FUNCTIONS
 =================================================================*/
/*=====================================
 * init_looking_for_broker
 * 
 * This function assumes we're connected to wifi, and sets up a client for MQTT connection.
 */
void init_looking_for_broker( void )
{ 
  Serial.print("Looking for broker: ");
  print_broker_addr();
  
  IPAddress broker(nv_data.broker_addr[0],nv_data.broker_addr[1],nv_data.broker_addr[2],nv_data.broker_addr[3]); 
  
  client.setServer(broker, 1883);
  client.setCallback(mqtt_callback);
}

/*=====================================
 * init_looking_for_broker
 * 
 * This function attempts to connect to the MQTT broker.
 */
state_type process_looking_for_broker( void )
{

   Serial.print("Attempting MQTT connection...");

  if (client.connect(nv_data.client_id))
  {
    
      init_active();

      return STATE_ACTIVE;
  } 
  else 
  {
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
      return STATE_LOOKING_FOR_BROKER;
    }
  
}

/*=======================================================================
 * ACTIVE STATE FUNCTIONS
 * 
 *   This example of active state will demonstrate periodically sending an MQTT message.
 *   We'll use the topic "letter", and periodically send an 'a' through 'z'.
 *======================================================================*/
 
/*============================================
 * init_active
 * 
 * Initialize our active state, specifically subscribing to the "letter" topic.
 */
void init_active( void )
{
   client.subscribe("letter");
}

/*=============================================
 * process_active
 * 
 * In the active state, we send a message every SEND_TIME_MS,
 * incrementing the letter being send from a to z.
 * 
 * Note we also need to call client.loop() in order to have the callback register,
 * which also means we want to avoid delays in here.
 */
#define SEND_TIME_MS 5000    // how often to send a message
state_type process_active( void )
{
  unsigned long current_time;
  static unsigned long last_sent_time=0;   
  static char current_letter[2]="a";
    
  // check to see if it's time to send the next message
  current_time = millis();
  if (current_time >= last_sent_time + SEND_TIME_MS)    
  {
    
    client.publish("letter", current_letter);
    if (current_letter[0] == 'z')
    { 
      current_letter[0] = 'a';
    }
    else
    {
      current_letter[0]++;
    }
    last_sent_time = current_time;
  }

  client.loop();

  return STATE_ACTIVE;
}

/*===========================================
 * SETUP
 */
void setup( void ) 
{
  Serial.begin(9600); 
  
  EEPROM.begin(sizeof(nv_data_type));
  EEPROM.get(0, nv_data);

  
  Serial.println();
  Serial.println("Initializing 8266 MQTT");
  Serial.print("Network: ");
  Serial.println(nv_data.ssid);
  Serial.print("Client id: ");
  Serial.println(nv_data.client_id);
  Serial.print("Broker addr: ");
  print_broker_addr();  
  Serial.println("Init complete");

}

/*===========================================
 * LOOP
 */
void loop()
{
  static state_type current_state=STATE_DISCONNECT;  

  switch (current_state)
  {
    case STATE_OFFLINE:
      current_state = process_offline_state();
    break;
    
    case STATE_DISCONNECT:
      // any serial input will kick us to offline.
      if (Serial.available())
      {
        Serial.println("Going to offline state...");
        init_offline_state();
        current_state = STATE_OFFLINE;
      }
      else
      {
        current_state = process_disconnect_state();
      }
    break;

    case STATE_LOOKING_FOR_BROKER:

      // any serial input will kick us to offline.
      if (Serial.available())
      {
        Serial.println("Going to offline state....");
        init_offline_state();
        current_state = STATE_OFFLINE;
      }
      else
      {
        current_state = process_looking_for_broker();
      }
    break;

    case STATE_ACTIVE:
      // any serial input will kick us to offline.
      if (Serial.available())
      {
        Serial.println("Going to offline state....");
        init_offline_state();
        current_state = STATE_OFFLINE;
      }
      else
      {
        current_state = process_active();
      }

    break;

    default:
      Serial.println("Unexpected STATE!!!");
  }  // end of switch on current state

}
//check!!!
