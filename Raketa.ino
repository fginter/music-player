#include <EEPROM.h>

/*

Otikova raketa
(c) 2011-2012 Filip Ginter

Built on top of countless examples / libraries

Credits in particular:

FAT16 ReadFile Example
SparkFun Electronics
Written by Ryan Owens
3/16/2010

...plus many more bits and pieces off the Net

*/

//Add libraries to support FAT16 on the SD Card.
//(Note: If you already have these libraries installed in the directory, they'll have to remove in order to compile this.)
#include <stdlib.h>
#include <SPI.h>
#include <byteordering.h>
#include <fat.h>
#include <fat_config.h>
#include <partition.h>
#include <partition_config.h>
#include <sd-reader_config.h>
#include <sd_raw.h>
#include <sd_raw_config.h>

//Define the pin numbers for SD card communication
#define CS    10
#define MOSI    11
#define MISO    12
#define SCK    13

#define TIMER_CLOCK_FREQ 1500000.0 //12M/8
#define MUSIC_RATE 20000
#define FILENAME "00_20.da"

//This is the amount of data to be fetched from the SD card for each read.
#define BUFFERHALFSIZE	512 // here is where the second half of the buffer starts
#define DOUBLEBUFFERSIZE 1024 //double buffer of 2x512
unsigned char buffer[DOUBLEBUFFERSIZE]; //Card read double buffer
unsigned int bufferPos=0; //Where, in the buffer, is the player?

#define LEFT 0
#define RIGHT 1

//1 means "dirty - reload whenever you can"
unsigned char bufferDirty=0 | (1<<RIGHT);



//Define the Arduino pin numbers for D/A convertor communication
#define DASEL 6
#define DACLK 7
#define DADATA 8

//Define the AVR pin numbers
#define AVR_DASEL 0 //PB0
#define AVR_DACLK 7 //PD7
#define AVR_DADATA 6 //PD6

struct fat_dir_struct* dd;		//FAT16 directory
struct fat_dir_entry_struct dir_entry;	//FAT16 directory entry (A.K.A. a file)

struct fat_fs_struct* fs;		//FAT16 File System
struct partition_struct* partition;	//FAT16 Partition

struct fat_file_struct * file_handle;	//FAT16 File Handle

unsigned char timerLoadValue=0;


/*
2000000Hz
/ 44100Hz -> 45

 */

unsigned char SetupTimer2(float timeoutFrequency){
  unsigned char result; //The timer load value.

  //Calculate the timer load value
  result=(int)((257.0-(TIMER_CLOCK_FREQ/timeoutFrequency))+0.5);
  //The 257 really should be 256 but I get better results with 257.

  //Timer2 Settings: Timer Prescaler /8, mode 0
  //Timer clock = 16MHz/8 = 2Mhz or 0.5us
  //The /8 prescale gives us a good range to work with
  //so we just hard code this for now.
  TCCR2A = 0;
  TCCR2B = 0<<CS22 | 1<<CS21 | 0<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 = 1<<TOIE2;

  //load the timer for its first cycle
  TCNT2=result;
  


  return(result);
}


void setup()
{
    //Set up the pins for SD card communication
    pinMode(CS, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(MISO, INPUT);
    pinMode(SCK, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(2, OUTPUT);
    digitalWrite(2,HIGH);
  
    //Set up the pins for D/A communication
    pinMode (DACLK, OUTPUT);
    pinMode (DADATA, OUTPUT);
    pinMode (DASEL, OUTPUT);
    digitalWrite(DASEL,HIGH);
    digitalWrite(DACLK, LOW);

    timerLoadValue=SetupTimer2(MUSIC_RATE);

    //randomSeed(100);

}

// D/A communication


//start D/A communication
inline void startDA() {
  PORTB &= (unsigned char) ~(1<<AVR_DASEL); //CS low
}

inline void stopDA() {
  PORTB |= (unsigned char) 1<<AVR_DASEL; //CS high
}

inline void write1() {
  PORTD |= (unsigned char)(1<<AVR_DADATA) | (unsigned char)(1<<AVR_DACLK); //data and clock to high 
  PORTD &= (unsigned char) ~(1<<AVR_DACLK); //clock to low
}

inline void write0() {
  PORTD &= (unsigned char) ~(1<<AVR_DADATA); //data low
  PORTD |= (unsigned char) 1<<AVR_DACLK; //clock to high 
  PORTD &= (unsigned char) ~(1<<AVR_DACLK); //clock to low
}

inline void writeDAByte_DAFORMAT() {
    //Writes one byte from the buffer and increments
    register unsigned char selector=0b10000000;
    register unsigned char v=buffer[bufferPos];
    while (selector) {
      (v&selector) ? write1() : write0();
      selector>>=1;
    }
    bufferPos++;
}


inline void play_DAFORMAT() {
  startDA();
  writeDAByte_DAFORMAT();
  writeDAByte_DAFORMAT();
  stopDA();

  if (bufferPos==DOUBLEBUFFERSIZE) {
      bufferPos=0;
  }
 
 }


ISR(TIMER2_OVF_vect) {
  unsigned char start=TCNT2;
  play_DAFORMAT();
  TCNT2=(TCNT2-start)+timerLoadValue;
}

unsigned char updateBuffer() {
  unsigned int bytes_read;
  if (bufferPos < BUFFERHALFSIZE && bufferDirty & (1<<RIGHT)) { //reading left and right side dirty
    bufferDirty |= (1<<LEFT); //set left half dirty
    bytes_read = fat_read_file(file_handle, buffer+BUFFERHALFSIZE, BUFFERHALFSIZE);
    bufferDirty &= ~(1<<RIGHT); //clean right half
    if (bytes_read<BUFFERHALFSIZE) {
      haltCircuit();
      return 1;  
    }

  }
  else if (bufferPos >= BUFFERHALFSIZE && bufferDirty & (1<<LEFT)) { //reading right and left side dirty
    bufferDirty |= (1<<RIGHT); //set right half dirty
    bytes_read = fat_read_file(file_handle, buffer, BUFFERHALFSIZE);
    bufferDirty &= ~(1<<LEFT); //clean left half
    if (bytes_read<BUFFERHALFSIZE) {
      haltCircuit();
      return 1;  
    }
  }
  return 0;
}

void haltCircuit() {
  digitalWrite(2,LOW);
}

/*void loop() {
  writeDAVal(65000);
  delayMicroseconds(1);
  writeDAVal(10);
  delayMicroseconds(1);
}*/

int nextSongNum(int total) {
    unsigned char res=EEPROM.read(0)+1;
    
    if (res>=total) {
        res=0;
    }
    
    EEPROM.write(0,res);
    return (int)res;
}
    

void loop() {
    char fileNameS[30]="";
    init_filesystem();	//Initialize the FAT16 file system on the SD card.
    int musicFiles=0;
    //
    
    while (get_next_filename(dd, fileNameS)) {
      if (fileNameS[0]=='r') {//raketa sound file
        musicFiles+=1;        
      }
    }
    
    sprintf(fileNameS, "r%02d_20.da", nextSongNum(musicFiles));//, (int)random(musicFiles));
    
    file_handle=open_file_in_dir(fs, dd, fileNameS);
    
    //Prime the left half of the buffer with data
    fat_read_file(file_handle, buffer, BUFFERHALFSIZE);

    while (!updateBuffer()) {
    }

    fat_close_file(file_handle);    
}

uint8_t find_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry)
{
	fat_reset_dir(dd);	//Make sure to start from the beginning of the directory!
    while(fat_read_dir(dd, dir_entry))
    {
        if(strcmp(dir_entry->long_name, name) == 0)
        {
            //fat_reset_dir(dd);
            return 1;
        }
    }

    return 0;
}

struct fat_file_struct* open_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name)
{
    struct fat_dir_entry_struct file_entry;
    if(!find_file_in_dir(fs, dd, name, &file_entry))
        return 0;

    return fat_open_file(fs, &file_entry);
}

char init_filesystem(void)
{
	//setup sd card slot 
	if(!sd_raw_init())
	{
		return 0;
	}

	//open first partition
	partition = partition_open(sd_raw_read,
									sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
									sd_raw_write,
									sd_raw_write_interval,
#else
									0,
									0,
#endif
									0
							   );

	if(!partition)
	{
		//If the partition did not open, assume the storage device
		//is a "superfloppy", i.e. has no MBR.
		partition = partition_open(sd_raw_read,
								   sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
								   sd_raw_write,
								   sd_raw_write_interval,
#else
								   0,
								   0,
#endif
								   -1
								  );
		if(!partition)
		{
			return 0;
		}
	}

	//Open file system
	fs = fat_open(partition);
	if(!fs)
	{
		return 0;
	}

	//Open root directory
	fat_get_dir_entry_of_path(fs, "/", &dir_entry);
	dd=fat_open_dir(fs, &dir_entry);
	
	if(!dd)
	{
		return 0;
	}
	return 1;
}

char get_next_filename(struct fat_dir_struct* cur_dir, char * new_file)
{
    //'dir_entry' is a global variable of type directory_entry_struct

    //Get the next file from the root directory
    if(fat_read_dir(cur_dir, &dir_entry))
    {
	sprintf(new_file, "%s", dir_entry.long_name);
        //Serial.println((const char *)new_file);
        return 1;
    }
    //If another file isn't found, return 0
    return 0;
}


