/******************************************************************************
                        SUPER SECURE GUITAR TUNER
			     Guitar tuner dengan pengamanan password
                   IF3111 - Platform Based Development
				 
   Oleh:
		13512028 Andre Susanto
		13512060 Adhika Sigit R
		13512072 Kanya Paramita

********************************************************************************/
#include "Arduino.h"
#include <Keypad.h>		// library keypad

// Variabel untuk Aktuator LED
unsigned long wkt;


// Variabel untuk Seven segment

int latchPin = 10;
int clockPin = 12;
int dataPin = 11;

byte data;			// digunakan untuk proses transfer ke shift register
byte dataArray[8];  // database huruf A sd G

// Variabel untuk matrix Keypad

const byte ROWS = 4; 
const byte COLS = 3; 
char keys[ROWS][COLS] = { {'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'} }; // definisi keypad

byte rowPins[ROWS] = {8, 7, 6, 5}; 	// Pin baris
byte colPins[COLS] = {4, 3, A3}; 	// Pin kolom

boolean locked = 1;			// apakah tunernya terkunci
String password = "1234";	// password untuk tuner
String tmp_pass = "";		// variabel untuk menyimpan password yang dimasukan pengguna

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// Variabel untuk membaca frequensi output OP-AMP
boolean clipping = 0;
byte newData 	= 0;
byte prevData = 0;
unsigned int time = 0;	// untuk menyimpan waktu (diperlukan untuk perhitungan frekuensi)
int timer[10];			// untuk menyimpan histori waktustorage for timing of events
int slope[10];			// untuk menyimpan histori slope
unsigned int totalTimer;// untuk menghitung perioda waktu
unsigned int period;	// untuk menghitung peroda gelombang
byte index = 0;			// index dari penyimpanan yang digunakan
float frequency;		// untuk menyimpan hasil perhitungan frekuensi
int maxSlope = 0;		// menyimpan maksimum slope
int newSlope;			// menyimpan slope data baru

// Variabel yang digunakan ketika OP-AMP menangkap frequensi yang ditargetkan
byte noMatch = 0;
byte slopeTol = 3;
int timerTol = 10;

unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;


volatile byte ampThreshold = 30; // Variabel kontrol kejernihan / noise filter


// Interrupt untk mengganti ampTreshold (push button)
void switchGain(){
  if (ampThreshold == 30){
     ampThreshold = 40;
     Serial.println("LOW GAIN MODE");
  }else{
     ampThreshold = 30; 
     Serial.println("HIGH GAIN MODE");
  }
  
}


void setup(){
  pinMode(latchPin, OUTPUT);
  
  // Membuat interrupt dengan tombol push button (di interrupt ketika off -> on)
  attachInterrupt(0, switchGain, RISING);
  
  // mengeset mode pin untuk Aktuator LED
  pinMode(A2, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  
  // Komunikasi dengan PC Pengguna
  Serial.begin(9600);
  
  
  ////////////////////////////////// INISIASI UNTUK PERHITUNGAN FREKUENSI DARI OP-AMP \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
  
  cli();
  
  // Sampling pada A0 dengan frekuensi 38.5kHz
 
  ADCSRA = 0;
  ADCSRB = 0;
  
  ADMUX |= (1 << REFS0);
  ADMUX |= (1 << ADLAR);
  
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0);
  ADCSRA |= (1 << ADATE);
  ADCSRA |= (1 << ADIE); 
  ADCSRA |= (1 << ADEN);
  ADCSRA |= (1 << ADSC);
  
  sei();
  
  ////////////////////////////////// SELESAI INISIASI OP-AMP \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
  
  
  
  
  // inisialisasi data seven segment
  
  dataArray[0] = 0xBE; //A
  dataArray[1] = 0xF8; //B
  dataArray[2] = 0xCC; //C
  dataArray[3] = 0xF2; //D
  dataArray[4] = 0xDC; //E
  dataArray[5] = 0x9C; //F
  dataArray[6] = 0xFC; //G
  dataArray[7] = 0x00; //Awal atau kosong
  
  
  // Print ucapan selamat datang ke PC pengguna
  Serial.println("WELCOME TO SECURE GUITAR TUNER");
  Serial.println("==============================");
  Serial.print("Please type password to continue (end with #): ");
  
  
  // Kosongkan Seven Segment
  data = dataArray[7];
  digitalWrite(latchPin, 0);
  shiftOut(dataPin, clockPin, data);
  digitalWrite(latchPin, 1); 
  
}

//// LIBRARY OP-AMP DETECTION

ISR(ADC_vect) {//when new ADC value ready
  
  prevData = newData;//store previous value
  newData = ADCH;//get value from A0
  if (prevData < 127 && newData >=127){//if increasing and crossing midpoint
    newSlope = newData - prevData;//calculate slope
    if (abs(newSlope-maxSlope)<slopeTol){//if slopes are ==
      //record new data and reset time
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){//new max slope just reset
        noMatch = 0;
        index++;//increment index
      }
      else if (abs(timer[0]-timer[index])<timerTol && abs(slope[0]-newSlope)<slopeTol){//if timer duration and slopes match
        //sum timer values
        totalTimer = 0;
        for (byte i=0;i<index;i++){
          totalTimer+=timer[i];
        }
        period = totalTimer;//set period
        //reset new zero index values to compare with
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1;//set index to 1
         noMatch = 0;
      }
      else{//crossing midpoint but not match
        index++;//increment index
        if (index > 9){
          reset();
        }
      }
    }
    else if (newSlope>maxSlope){//if new slope is much larger than max slope
      maxSlope = newSlope;
      time = 0;//reset clock
      noMatch = 0;
      index = 0;//reset index
    }
    else{//slope not steep enough
      noMatch++;//increment no match counter
      if (noMatch>9){
        reset();
      }
    }
  }
    
  if (newData == 0 || newData == 1023){//if clipping
    clipping = 1;//currently clipping
    Serial.println("clipping");
  }
  
  time++;//increment timer at rate of 38.5kHz
  
  ampTimer++;//increment amplitude timer
  if (abs(127-ADCH)>maxAmp){
    maxAmp = abs(127-ADCH);
  }
  if (ampTimer==1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }
  
}

// reset op-amp detection
void reset(){
  index = 0;
  noMatch = 0;
  maxSlope = 0;
}


void checkClipping(){//manage clipping indication
  if (clipping){//if currently clipping
    clipping = 0;
  }
}

/////////////////// MAIN LOOP \\\\\\\\\\\\\\\\\\\\\\\

void loop(){
  checkClipping();
  
  if (millis() - wkt > 2000){	// setelah 2 detik tidak ada aktifitas tuning, matikan LED Aktuator
      digitalWrite(A2, LOW);
      digitalWrite(A4, LOW);
      digitalWrite(A5, LOW);
  }
  
  
  if (locked){ // Dijalankan ketika TUNER terkunci
      char key = keypad.getKey(); // terima input dari keypad

      if (key != NO_KEY){
        if (key == '#'){
          if (tmp_pass.equals(password)){ // Password benar
            Serial.println("\nPassword OK! \n\nYou are authorized to use this tuner!");
            locked = 0; // buka lock
          }else{
            Serial.print("\nPassword Not OK!\nPlease type again : ");
          }
          tmp_pass = "";
        }else{
          tmp_pass += key;
          Serial.print('*');
        }
        
      }
  }else{ // Dijakankan ketika TUNER sudah terbuka
  
      if (checkMaxAmp>ampThreshold){ // jika frequensi yang didapatkan tidak noise 
	  
        frequency = 38462/float(period); // Hitung frekuensi berdasar perioda
        float selisih = 0; // selisih frekuensi terhadap target frekuensi
        
		// Memilih target frekuensi berdasar frekuensi yang terdekat
        if (frequency > 70 && frequency < 100){
            selisih = (frequency - 82.4);
            data = dataArray[4];
        }else if (frequency > 100 && frequency < 125){
            selisih = (frequency - 110);
            data = dataArray[0];
        }else if (frequency > 125 && frequency < 165){
            selisih = (frequency - 146.8);
            data = dataArray[3];
        }else if (frequency > 165 && frequency < 220){
            selisih = (frequency - 196);
            data = dataArray[6]; 
        }else if (frequency > 220 && frequency < 290){
            selisih = (frequency - 246.9) ;
            data = dataArray[1];
        }else if (frequency > 290 && frequency < 400){
            selisih = (frequency - 329.6);
            data = dataArray[4];
        }
        
		// Tampilkan progress tuning gitar
        if (frequency <= 400){
            Serial.print("Progress: ");
            Serial.print(selisih);
            Serial.println(" Hz");
            wkt = millis();
            
			// indikator aktuator
            if (abs(selisih) < 1){
              digitalWrite(A2, LOW);
              digitalWrite(A4, HIGH);
              digitalWrite(A5, LOW);
            }else if (selisih < 0){
              digitalWrite(A2, HIGH);
              digitalWrite(A4, LOW);
              digitalWrite(A5, LOW);
            }else if(selisih > 0){
              digitalWrite(A2, LOW);
              digitalWrite(A4, LOW);
              digitalWrite(A5, HIGH);
            }
        }
        
		// Push CHORD ke Seven segment
        digitalWrite(latchPin, 0);
        shiftOut(dataPin, clockPin, data);
        digitalWrite(latchPin, 1); 
      
      }
    
      delay(100);
  }
}

/////// SHIFT KE SHIFT REGISTER UNTUK DITAMPILKAN KE SEVEN SEGMENT 
void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
  int i=0;
  int pinState;
  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, OUTPUT);

  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  for (i=7; i>=0; i--)  {
    digitalWrite(myClockPin, 0);
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else { 
      pinState= 0;
    }

    digitalWrite(myDataPin, pinState);
    digitalWrite(myClockPin, 1);
    digitalWrite(myDataPin, 0);
  }

  digitalWrite(myClockPin, 0);
}


