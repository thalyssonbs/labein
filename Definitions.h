#define   sensorPin          2
#define   heaterPin          3
#define   humidifierPin      0

#define   tempOffset         0.5
#define   umidOffset         -0.5
#define   tempHist           0.1
#define   umidHist           0.1
#define   EVENT_WAIT_TIME    30000



#define FEEDBACK_LED_IS_ACTIVE_LOW // The LED on my board (D4) is active LOW
#define IR_RECEIVE_PIN          14 // D5
#define IR_SEND_PIN             1 // D6 - D4/pin 2 is internal LED
#define _IR_TIMING_TEST_PIN     13 // D7
#define APPLICATION_PIN          0 // D3

#define tone(...) void()      // tone() inhibits receive timer
#define noTone(a) void()
#define TONE_PIN                42 // Dummy for examples using it
