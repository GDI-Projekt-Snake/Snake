/* This Library commands an ESP8266 with stock AT-firmware
 * to connect to an acces point.
 * It then starts listening on a TCP-port and will accept
 * exactly one connection to which it provides an Arduino-stream
 * interface.
 *
 * Known caveats:
 *  - Everything will break if you do not periodically call
 *    available() or read(). Periodically means every few
 *    milliseconds. Use delay() and your program will break,
 *    you have been warned.
 *
 *  - It is insecure AF, this library was designed to be
 *    simple, in writing and using.
 *    Every single best-practice in writing secure C++
 *    was ignored.
 *    DO NOT USE THIS LIBRARY IN PRODUCTION SOFTWARE! */

#include <Arduino.h>
#include <Stream.h>

#include "DumbServer.h"

#define ESP_SUCESS_READY ((char *)"ready\r\n")
#define ESP_SUCESS_OK ((char *)"OK\r\n")
#define ESP_SUCESS_PKG ((char *)"+IPD,")
#define ESP_SUCESS_IPQ ((char *)"+CIPSTA_CUR:ip:\"")
#define ESP_SUCESS_SENT ((char *)"SEND OK\r\n")

EspServer::EspServer(void)
{
  _esp_serial= NULL;
  connection_id= -1;
  rem_msg_len= 0;
}

void EspServer::begin(Stream *esp_serial, const char* ssid, const char *pass, uint16_t port)
{
  _esp_serial= esp_serial;
  _esp_serial->setTimeout(60000);

  reset();
  connect_wifi(ssid, pass);
  setup_server(port);
}

void EspServer::flush_in_buff()
{
  while(_esp_serial->available()) {
    _esp_serial->read();
  }
}

char EspServer::blockread()
{
  while(!_esp_serial->available());

  return(_esp_serial->read());
}

void EspServer::expect(char *msg)
{
  char *exp= msg;

  while(*exp) {
    if (blockread() != *exp) {
      exp= msg;
    }
    else {
      exp++;
    }
  }
}

void EspServer::reset()
{
  _esp_serial->println("AT+RST");
  expect(ESP_SUCESS_READY);

  flush_in_buff();

  // Disable command echo
  _esp_serial->println("ATE0");
  expect(ESP_SUCESS_OK);
}

void EspServer::connect_wifi(const char *ssid, const char* pwd)
{
  // Set station mode
  _esp_serial->println("AT+CWMODE_CUR=1");
  expect(ESP_SUCESS_OK);

  // Connect to AP
  _esp_serial->print("AT+CWJAP_CUR=\"");
  _esp_serial->print(ssid);
  _esp_serial->print("\",\"");
  _esp_serial->print(pwd);
  _esp_serial->println("\"");
  expect(ESP_SUCESS_OK);

  // Get an IP using DHCP
  _esp_serial->println("AT+CIFSR");
  expect(ESP_SUCESS_OK);
}

void EspServer::setup_server(uint16_t port)
{
  /* Configure for multiple connections
   * (necessary to run as server) */
  _esp_serial->println("AT+CIPMUX=1");
  expect(ESP_SUCESS_OK);

  // Start listening for connections
  _esp_serial->print("AT+CIPSERVER=1,");
  _esp_serial->println(port);
  expect(ESP_SUCESS_OK);
}

void EspServer::my_ip(char *buf, size_t buflen)
{
  _esp_serial->println("AT+CIPSTA_CUR?");
  expect(ESP_SUCESS_IPQ);

  memset(buf, 0, buflen);
  _esp_serial->readBytesUntil('"', buf, buflen - 1);

  expect(ESP_SUCESS_OK);
}

bool EspServer::connected()
{
  /* available() will, as a side-effect check if
   * the connection status changed */
  available();

  /* Return true if there is currently a
   * client connected */
  return(connection_id >= 0);
}

int EspServer::available()
{
  int ser_available= _esp_serial->available();

  if(rem_msg_len) {
    // There is a message in transit

    return(min(rem_msg_len, ser_available));
  }

  if(ser_available) {
    char bte= _esp_serial->peek();

    /* CONNECT/DISCONNECT messages
     * start with a number */
    if(bte >= '0' && bte <= '9') {
      /* Read the connection ID and skip
       * to the first part of the command */
      int id= _esp_serial->parseInt();

      expect((char *)",C");

      /* Read the character that lets us determine
       * if a connection was opened or closed */
      char cmd= blockread();

      if(cmd == 'O') {
        // Command is ?,CONNECT

        if(connection_id >= 0) {
          /* Only allow one open connection,
           * close the new one */

          _esp_serial->print("AT+CIPCLOSE=");
          _esp_serial->println(id);
        }
        else {
          connection_id= id;
        }
      }

      if(cmd == 'L') {
        // Command is ?,CLOSED

        if(id == connection_id) {
          connection_id= -1;
        }
      }

      expect((char *)"\r\n");
    }
    else if(bte == '+') {
      char cmd[5]= {0};
      _esp_serial->readBytes(cmd, 4);

      int id= _esp_serial->parseInt();
      int len= _esp_serial->parseInt();

      expect((char *)":");

      if(id == connection_id) {
        rem_msg_len= len;
      }
    }
    else {
      _esp_serial->read();
    }
  }


  return(0);
}

int EspServer::peek()
{
  if(available()) {
    return(_esp_serial->peek());
  }
  else {
    return(-1);
  }
}

int EspServer::read()
{
  if(available()) {
    rem_msg_len -= 1;

    return(_esp_serial->read());
  }
  else {
    return(-1);
  }
}

void EspServer::flush()
{
  _esp_serial->flush();
}

size_t EspServer::write(const uint8_t *buffer, size_t size)
{
  while(!connected());

  _esp_serial->print("AT+CIPSEND=");
  _esp_serial->print(connection_id);
  _esp_serial->print(",");
  _esp_serial->println(size);

  expect((char *)">");

  _esp_serial->write(buffer, size);
  expect((char *)ESP_SUCESS_SENT);

  return(size);
}
