uint8_t da=0<<3; // device address

typedef struct pack{
  uint8_t f, a, l; // flags, address, length
  int16_t di; // data index
  unsigned long time;
  uint8_t d[64]; // data
} pack;

typedef struct ifst{
  uint8_t ia, trp, rep, synci, ds; // interface address, transmit pin, receive pin
  unsigned long sync[6]; unsigned long syncavrg;
  pack pr; // packet receive
  pack pt; // packet transmit
} ifst;

ifst ifs[7]={{0}};


int initif(uint8_t in, uint8_t trp, uint8_t rep){

if(in<1 || in>7) return 1;

if(trp<2 || rep<2 || trp>19 || rep>19) return 1;

for(int i=0;i<7;i++){
  if(ifs[i].trp==trp || ifs[i].rep==rep || ifs[i].trp==rep || ifs[i].rep==trp) return 1;
}

ifs[in-1].ia=da|in;
ifs[in-1].trp=trp;
ifs[in-1].rep=rep;

pinMode(trp, OUTPUT);
pinMode(rep, INPUT);

if(rep<=7){
  PCICR|=1<<2;
  PCMSK2|=1<<rep;
}else if(rep<=13){
  PCICR|=1<<0;
  PCMSK0|=1<<(rep-8);
}else{
  PCICR|=1<<1;
  PCMSK1|=1<<(rep-14);
}

return 0;
}

volatile uint8_t ios0, ios1, ios2; // interrupt old state
volatile uint8_t ics0, ics1, ics2; // interrupt current state

ISR(PCINT0_vect){
  ics0=ios0^(PINB&PCMSK0);
  ios0=(PINB&PCMSK0);

  for(int i=0;i<8;i++){
    if((ics0>>i)&1){
      for(int j=0;j<8;j++){
        if(ifs[j].rep==i+8){
          if(ifs[j].synci>5){
            ifs[j].synci=0;
            ifs[j].syncavrg=(ifs[j].sync[1]+ifs[j].sync[2]+ifs[j].sync[3]+ifs[j].sync[4]+ifs[j].sync[5])/5;
            PCMSK0&=~(uint8_t)(1<<i);
          }else if(ifs[j].synci==0){
            ifs[j].sync[ifs[j].synci]=micros();
          }else{
            ifs[j].sync[ifs[j].synci]=micros()-ifs[j].sync[ifs[j].synci-1];
          }
        ifs[j].synci++;
        }
      }
    } 
  }
}

ISR(PCINT1_vect){
  ics1=(ios1^(PINC&PCMSK1))&PINC;
  ios1=(PINC&PCMSK1);

  for(int i=0;i<8;i++){
    if((ics1>>i)&1){
      for(int j=0;j<8;j++){
        if(ifs[j].rep==i+14){

        if(ifs[j].synci>5){
          ifs[j].synci=0;
          ifs[j].syncavrg=(ifs[j].sync[1]+ifs[j].sync[2]+ifs[j].sync[3]+ifs[j].sync[4]+ifs[j].sync[5])/5;
          PCMSK0&=~(uint8_t)(1<<i);
        }else if(ifs[j].synci==0){
          ifs[j].sync[ifs[j].synci]=micros();
        }else{
          ifs[j].sync[ifs[j].synci]=micros()-ifs[j].sync[ifs[j].synci-1];
        }
        ifs[j].synci++;
        }
      }
    }
  }
}

ISR(PCINT2_vect){
  ics2=(ios2^(PIND&PCMSK2))&PIND;
  ios2=(PIND&PCMSK2);

  for(int i=0;i<8;i++){
    if((ics0>>i)&1){
      for(int j=0;j<8;j++){
        if(ifs[j].rep==i){

        if(ifs[j].synci>5){
          ifs[j].synci=0;
          ifs[j].syncavrg=(ifs[j].sync[1]+ifs[j].sync[2]+ifs[j].sync[3]+ifs[j].sync[4]+ifs[j].sync[5])/5;
          PCMSK0&=~(uint8_t)(1<<i);
        }else if(ifs[j].synci==0){
          ifs[j].sync[ifs[j].synci]=micros();
        }else{
          ifs[j].sync[ifs[j].synci]=micros()-ifs[j].sync[ifs[j].synci-1];
        }
        ifs[j].synci++;
        }
      }
    }
  }
}
void setup(){
  initif(1, 2, 3);
  initif(2, 6, 7);
  Serial.begin(9600);
}

#define BITL 1000

#define PER 1000

void loop(){
  static unsigned long perc=0;
  
  for(int i=0; i<2; i++){
    if(ifs[i].ds){
      if(micros()-ifs[i].pt.time>=(BITL/2)){
        ifs[i].pt.time=micros();
        if(ifs[i].pt.di<0){
          digitalWrite(ifs[i].trp, (-ifs[i].pt.di)%2);
        }
        else if(ifs[i].pt.di/2<8){
          //Serial.println(ifs[i].pt.di/2);
          digitalWrite(ifs[i].trp, (ifs[i].pt.f>>(7-(ifs[i].pt.di/2)))&1);
          //Serial.println((ifs[i].pt.f>>(7-ifs[i].pt.di/2))&1);
        }
        else if(ifs[i].pt.di/2<16)
          digitalWrite(ifs[i].trp, (ifs[i].pt.a>>(15-ifs[i].pt.di/2))&1);
        else if(ifs[i].pt.di/2<24)
          digitalWrite(ifs[i].trp, (ifs[i].pt.l>>(23-ifs[i].pt.di/2))&1);
        else{
          digitalWrite(ifs[i].trp, (ifs[i].pt.d[(ifs[i].pt.di/2)/8]>>(7-(ifs[i].pt.di/2)%8))&1);
          if(!((ifs[i].pt.d[(ifs[i].pt.di/2)/8]>>(7-(ifs[i].pt.di/2)%8))&1)){
            ifs[i].ds=0;
            break;
          }
        }
        ifs[i].pt.di++;
      }
    }else{
        ifs[i].pt.f=0b01101101;
        ifs[i].pt.a=0b01010101;
        ifs[i].pt.l=0b10000111;
        ifs[i].pt.d[0]=0;
        ifs[i].pt.di=-13;
    }
      if(millis()-perc>=1000){
      perc=millis();
      ifs[i].pt.time=micros();
      ifs[i].ds=1;
    }
  }
}