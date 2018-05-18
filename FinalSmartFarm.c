
#include <wiringPi.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h>
#include <mysql/mysql.h>

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#include <math.h>

#define CS_MCP3208 8       // BCM_GPIO_8

#define SPI_CHANNEL 0
#define SPI_SPEED   1000000   // 1MHz

#define VCC         4.8       // Supply Voltage

// define farm 
#define MAXTIMINGS 85
#define RETRY 5

//#define PUMP	21 // BCM_GPIO 5

#define FAN	6 // BCM_GPIO 6
#define MOTORCONTROL 5 // BCM_GPIO 13
#define RGBLEDPOWER  24 //BCM_GPIO 19
#define RED	2
#define GREEN 3 
#define BLUE 4

#define LIGHTSEN_OUT 2  //gpio27 - J13 connect

typedef struct __myarg_t {
	int a;
	int b;
}myarg_t;

void sig_handler(int sig);
int ret_humid, ret_temp;

static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};

int read_dht22_dat_temp();
int read_dht22_dat_humid();
int get_temperature_sensor();
int get_humidity_sensor();
int get_light_sensor();

// here
#define MAX 100000
#define DBHOST "115.68.228.55"
#define DBUSER "root"
#define DBPASS "wownsdnwnalstj"
#define DBNAME "demofarmdb"

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;
int buffer[MAX];
int buffer2[MAX];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;

void sig_handler(int sig)
{
	digitalWrite(FAN,0);
	digitalWrite(RED,0);
	digitalWrite(GREEN, 0);
	digitalWrite(BLUE, 0);
	exit(0);
}
void put(int value,int value2)
{
	buffer[fill_ptr] = value;
	buffer2[fill_ptr] = value2;
	fill_ptr = (fill_ptr + 1) % MAX;
	count++;
}

myarg_t get()
{
	myarg_t m1;
	m1.a = buffer[use_ptr];
	m1.b = buffer2[use_ptr];
	use_ptr = (use_ptr + 1 ) %MAX;
	count--;
	return m1;
}

pthread_cond_t pro,con,TurnFan, TurnLED;
pthread_mutex_t mutex, mutex2, mutex3;

void *TurnOnFAN(void *arg)
{
	while(1)
	{
		pthread_cond_wait(&TurnFan, &mutex2);
		printf("나 는팬이야\n");
		digitalWrite(FAN,1);
		sleep(5);
		digitalWrite(FAN,0);
	}
}

void *TurnONLED(void *arg)
{
	while(1)
	{
		pthread_cond_wait(&TurnLED, &mutex3);
		digitalWrite(RED,1);
		digitalWrite(GREEN,0);
		digitalWrite(BLUE,0);
		delay(100);
		digitalWrite(RED,0);
		digitalWrite(GREEN,1);
		digitalWrite(BLUE,0);
		delay(100);
		digitalWrite(RED,0);
		digitalWrite(GREEN,0);
		digitalWrite(BLUE,1);
		delay(100);	
	}
}

void *producer(void *arg)
{
	int i;
	int tempValue;
	int lightValue;

	for(i=0; i <MAX; i++)
	{
		delay(1000);
		pthread_mutex_lock(&mutex);
		
		while(count == MAX)
			pthread_cond_wait(&pro, &mutex);
		
		tempValue = read_dht22_dat_temp();
		lightValue = get_light_sensor();
		
		printf("temperature : %d lightSenSor : %u\n", tempValue, lightValue);
		put(tempValue, lightValue);

		if(tempValue >= 28)
			pthread_cond_signal(&TurnFan);

		if(lightValue >= 1000)
			pthread_cond_signal(&TurnLED);
		
		else
		{
			digitalWrite(RED, 0);
			digitalWrite(GREEN,0);
			digitalWrite(BLUE,0);
		}

		pthread_cond_signal(&con);
		pthread_mutex_unlock(&mutex);	
	}
}
void *consumer(void *arg)
{
	int i;
	myarg_t m;
	
	for(i = 0 ; i< MAX; i++)
	{
		delay(10000);
		pthread_mutex_lock(&mutex);

		while(count == 0)
			pthread_cond_wait(&con, &mutex);

		m = get();

		printf("I am consumer: fan %d %d \n", m.a, m.b);
		
		connector = mysql_init(NULL);
		if(!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
		{
			fprintf(stderr, "%s\n", mysql_error(connector));
			return 0;
		}
		
		printf("mysql opened\n");
		char query[1024];
		sprintf(query, "insert into thl values (now(), %d, %d )",m.a, m.b);
		if(mysql_query(connector, query))
		{
			fprintf(stderr, "%s\n", mysql_error(connector));
			printf("Write DB error \n");
		}
		printf("값넣었다\n");
		pthread_cond_signal(&pro);
		pthread_mutex_unlock(&mutex);
	}
}
int read_mcp3208_adc(unsigned char adcChannel)
{
  unsigned char buff[3];
  int adcValue = 0;

  buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
  buff[1] = ((adcChannel & 0x07) << 6);
  buff[2] = 0x00;

  digitalWrite(CS_MCP3208, 0);  // Low : CS Active

  wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

  buff[1] = 0x0F & buff[1];
  adcValue = ( buff[1] << 8) | buff[2];

  digitalWrite(CS_MCP3208, 1);  // High : CS Inactive

  return adcValue;
}


int main (void)
{
	signal(SIGINT, (void*)sig_handler);
  int adcChannel  = 0;
  int adcValue[8] = {0};

  if(wiringPiSetup() == -1)
	  exit(EXIT_FAILURE);

  if(wiringPiSetupGpio() == -1)
  {
    fprintf (stdout, "Unable to start wiringPi: %s\n", strerror(errno));
    return 1 ;
  }

  if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
  {
    fprintf (stdout, "wiringPiSPISetup Failed: %s\n", strerror(errno));
    return 1;
  }

  if(setuid(getuid()) < 0)
  {
	  perror("Dropping Fauiled\n");
	  exit(EXIT_FAILURE);
  }
  
  pinMode(CS_MCP3208, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(RGBLEDPOWER, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE,OUTPUT);

  connector = mysql_init(NULL);
  if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
  {
    fprintf(stderr, "%s\n", mysql_error(connector));
    return 0;
  }

  printf("MySQL(rpidb) opened.\n");
  pthread_t produce, consumers, turnonFan, turnonLed;

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&pro, NULL);
  pthread_cond_init(&con, NULL);
  pthread_cond_init(&TurnFan, NULL);
  pthread_cond_init(&TurnLED, NULL);
	
  pthread_create(&produce, NULL, producer, NULL);
  pthread_create(&consumers, NULL, consumer, NULL);
  pthread_create(&turnonFan,NULL, TurnOnFAN,NULL);
  pthread_create(&turnonLed,NULL, TurnONLED,NULL);

  pthread_join(produce, NULL);
  pthread_join(consumers, NULL);
  pthread_join(turnonFan,NULL);
  pthread_join(turnonLed,NULL);

  mysql_close(connector);
  pthread_cond_destroy(&pro);
  pthread_cond_destroy(&con);
  pthread_cond_destroy(&TurnFan);
  pthread_cond_destroy(&TurnLED);
  return 0;
}

int get_temperature_sensor()
{
	int received_temp;
	int _retry = RETRY;

	DHTPIN = 11;

	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;
	
	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}
	while (read_dht22_dat_temp() == 0 && _retry--)
	{
		delay(500); // wait 1sec to refresh
	}
	received_temp = ret_temp ;
	printf("Temperature = %d\n", received_temp);
	
	return received_temp;
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}
int read_dht22_dat_temp()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40); 
  // prepar
  
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_humid = (int)h;
		ret_temp = (int)t;
		//printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		//printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);
		
    return ret_temp;
  }
  else
  {
    printf("Data not good, skip\n");
    return 0;
  }
}


int get_humidity_sensor()
{
	int received_humid;
	int _retry = RETRY;

	DHTPIN = 11;

	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;
	
	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	while (read_dht22_dat_humid() == 0 && _retry--) 
	{
		delay(500); // wait 1sec to refresh
	}

	received_humid = ret_humid;
	printf("Humidity = %d\n", received_humid);
	
	return received_humid;
}

int read_dht22_dat_humid()
{
	uint8_t laststate = HIGH;
	uint8_t counter = 0;
	uint8_t j = 0, i;

	dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

	// pull pin down for 18 milliseconds
	pinMode(DHTPIN, OUTPUT);
	digitalWrite(DHTPIN, HIGH);
	delay(10);
	digitalWrite(DHTPIN, LOW);
	delay(18);
	// then pull it up for 40 microseconds
	digitalWrite(DHTPIN, HIGH);
	delayMicroseconds(40); 
	// prepare to read the pin
	pinMode(DHTPIN, INPUT);

	// detect change and read data
	for ( i=0; i< MAXTIMINGS; i++) 
	{
		counter = 0;
		while (sizecvt(digitalRead(DHTPIN)) == laststate) 
		{
			counter++;
			delayMicroseconds(1);
			if (counter == 255) {
			break;
		}
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) 
	{
		// shove each bit into the storage bytes
		dht22_dat[j/8] <<= 1;
		if (counter > 16)
			dht22_dat[j/8] |= 1;
		j++;
    }
}

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
	if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_humid = (int)h;
		ret_temp = (int)t;
		printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);
		
    return ret_humid;
	}
	else
	{
		printf("Data not good, skip\n");
		return 0;
	}
}

int wiringPicheck(void)
{
	if (wiringPiSetup () == -1)
	{
		fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
		return 1 ;
	}
}

int get_light_sensor(void)
{
	unsigned char adcChannel = 0;
	
	unsigned char buff[3];
	int adcValue =0;

	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
	buff[1] = ((adcChannel & 0x07) << 6 );
	buff[2] = 0x00;

	digitalWrite(CS_MCP3208 , 0);
	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

	buff[1] = 0x0f & buff[1];
	adcValue = (buff[1] << 8 ) | buff[2];

	digitalWrite(CS_MCP3208, 1 );

	return adcValue;

}



