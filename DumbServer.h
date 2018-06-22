#pragma once

#include <Arduino.h>
#include <Stream.h>

class EspServer : public Stream
{
private:
  Stream *_esp_serial;
  int connection_id;
  int rem_msg_len;

  void flush_in_buff();
  void reset();
  char blockread();
  void connect_wifi(const char *ssid, const char* pwd);
  void setup_server(uint16_t port);
  void expect(char *msg);

public:
  EspServer(void);

  void begin(Stream *esp_serial, const char* ssid, const char *pass, uint16_t port);
  void my_ip(char *buf, size_t buflen);
  bool connected();

  virtual int available();
  virtual int peek();
  virtual int read();
  virtual void flush();
  virtual size_t write(const uint8_t *buffer, size_t size);

  inline size_t write(uint8_t n) { return write(&n, 1); }
  inline size_t write(unsigned long n) { return write((uint8_t)n); }
  inline size_t write(long n) { return write((uint8_t)n); }
  inline size_t write(unsigned int n) { return write((uint8_t)n); }
  inline size_t write(int n) { return write((uint8_t)n); }

  using Print::write;
};
