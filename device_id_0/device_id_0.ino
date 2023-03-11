uint8_t da=0<<3; // device address

typedef struct pack{
  uint8_t f, a, l, ds; // flags, address, length
  int16_t di; // data index
  unsigned long time;
  uint8_t d[16]; // data
} pack;

typedef struct ifst{
  uint8_t ia, trp, rep, synci; // interface address, transmit pin, receive pin
  unsigned long sync[6]; unsigned long syncavrg;
  pack pr; // packet receive
  pack pt; // packet transmit
} ifst;

ifst ifs[7]={{0}};

uint8_t rt[32]={0};

#define BITL 1000

#define PER 1000

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

volatile uint8_t ios0, ios1, ios2=0; // interrupt old state
volatile uint8_t ics0, ics1, ics2=0; // interrupt current state

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
            break;
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
          PCMSK1&=~(uint8_t)(1<<i);
          break;
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
  for(int i=0;i<8;i++){ //pin check
    if((ics2>>i)&1){ 
     // Serial.print("updated pin "); Serial.println(i);
      
     for(int j=0;j<2;j++){ 
        if(ifs[j].rep==i){
          //Serial.print(" pin matched "); Serial.print(i);Serial.print(" ");Serial.println(ifs[j].synci);
          if(ifs[j].synci>=6){
            ifs[j].synci=0;
            PCMSK2&=~(uint8_t)(1<<i);
            ifs[j].pr.ds=1;
            //Serial.print("  end sync ");Serial.println("ne be majka ti majka tiiii");
          }
          ifs[j].synci++;
        }
      }
    }
  }
}
#define REED 11
void setup(){
  pinMode(REED, INPUT_PULLUP);
  rt[0]=0b00000000;
  initif(1, 2, 3);
  //initif(2, 4, 5);
  Serial.begin(9600);
}
unsigned long perc=0;
void loop(){
  //char sensor[2];sensor[0] = digitalRead(REED) + '0';sensor[1]='\0';
  char *sensor="Data";


  for(int i=0; i<2; i++){
    if(ifs[i].pt.ds){
      if(micros()-ifs[i].pt.time>=(BITL/2)){
        ifs[i].pt.time=micros();
        if(ifs[i].pt.di<0){
          digitalWrite(ifs[i].trp, (-ifs[i].pt.di)%2);
        }
        else if(ifs[i].pt.di/2<8){
          //Serial.println(ifs[i].pt.di/2);
          digitalWrite(ifs[i].trp, (ifs[i].pt.f>>(7-(ifs[i].pt.di/2)))&1);
        }
        else if(ifs[i].pt.di/2<16)
          digitalWrite(ifs[i].trp, (ifs[i].pt.a>>(15-ifs[i].pt.di/2))&1);
        else if(ifs[i].pt.di/2<24)
          digitalWrite(ifs[i].trp, (ifs[i].pt.l>>(23-ifs[i].pt.di/2))&1);
        else{
            digitalWrite(ifs[i].trp, (ifs[i].pt.d[((ifs[i].pt.di)/2-24)/8]>>(7-((ifs[i].pt.di)/2-24)%8))&1);
            if(((ifs[i].pt.di)/2-24)%8==7 && (!ifs[i].pt.d[((ifs[i].pt.di)/2-24)/8])){
              ifs[i].pt.ds=0;
              //continue;
            }
        }
        ifs[i].pt.di++;
      }
    }else if(i==0){
        ifs[i].pt.f=0b00000100;
        ifs[i].pt.a=0b00001001;
        memcpy(ifs[i].pt.d, sensor, strlen(sensor)+1);
        ifs[i].pt.l=strlen(ifs[i].pt.d);
        ifs[i].pt.di=-13;
    }
  }
  for(int i=0; i<1; i++){
    if(ifs[i].pr.ds){
     if(ifs[i].pr.di==-1){
      ifs[i].pr.time=micros();
      ifs[i].pr.di++;
     }else if(micros()-ifs[i].pr.time>=BITL){
      ifs[i].pr.time=micros();
      //Serial.print("Device id: ");
      //Serial.println(ifs[i].pr.di);
        if(ifs[i].pr.di<8){
          ifs[i].pr.f|=digitalRead(ifs[i].rep)<<(7-ifs[i].pr.di);
        }
        else if(ifs[i].pr.di<16)
          ifs[i].pr.a|=digitalRead(ifs[i].rep)<<(15-ifs[i].pr.di);
        else if(ifs[i].pr.di<24)
          ifs[i].pr.l|=digitalRead(ifs[i].rep)<<(23-ifs[i].pr.di);
        else{
          if(ifs[i].pr.di-24>=(ifs[i].pr.l+1)*8 || ifs[i].pr.di-24>=32*8){
                      
            if(ifs[i].rep<=7){
              PCMSK2|=1<<ifs[i].rep;       PCMSK0|=1<<(ifs[i].rep-8);
            }else{
              PCMSK1|=1<<(ifs[i].rep-14);
            }



            ifs[i].pr.ds=0;
            //ifs[i].pr.di=-1;

            break;
          }
          ifs[i].pr.d[(ifs[i].pr.di-24)/8]|=digitalRead(ifs[i].rep)<<(7-(ifs[i].pr.di-24)%8);
        }
        ifs[i].pr.di++;
     }
    }else{
      // Serial.print(da);
      // Serial.println(ifs[i].pr.a);
      // Serial.println(ifs[i].pr.a&(0b11111000));
      if(ifs[i].pr.a!=0){
        if( (ifs[i].pr.a&(0b11111000))==da){
              Serial.print("Bit length: ");
              Serial.println(ifs[i].pr.di);
                          ifs[i].pr.di=-1;
              Serial.print("Flags: ");
              Serial.println(ifs[i].pr.f);
              Serial.print("Address: ");
              Serial.println(ifs[i].pr.a);
              Serial.print("Length: ");
              Serial.println(ifs[i].pr.l);
              Serial.print("Data: ");
              Serial.println((char *)ifs[i].pr.d);
                                      //     for(int k=0;k<=ifs[i].pr.l;k++){
                                      //     Serial.print((char)ifs[i].pr.d[k]);
                                      //     Serial.print(ifs[i].pr.d[k], BIN);
                                      //     Serial.print(" ");
                                      //     }
                                      //Serial.println();


              Serial.println("___________________________________");
        }else{
          for(int j=0;j<32;j++){
            if(!rt[j]) break;
            if((ifs[i].pr.a&(0b11111000))==(rt[j]&(0b11111000))){
                      ifs[(rt[j]&0b111)-1].pt.f=ifs[i].pr.f;
                      ifs[(rt[j]&0b111)-1].pt.a=ifs[i].pr.a;
                                
                      memcpy(ifs[(rt[j]&0b111)-1].pt.d, ifs[i].pr.d, 5);
                                          //memcpy(ifs[(rt[j]&0b111)-1].pt.d, "KURWA", 6);
                      ifs[(rt[j]&0b111)-1].pt.l=4; // ifs[i].pr.l+1
                                        Serial.println((char *)ifs[(rt[j]&0b111)-1].pt.d);
                                        for(int k=0;k<=ifs[(rt[j]&0b111)-1].pt.l;k++){
                                          Serial.print((char)ifs[(rt[j]&0b111)-1].pt.d[k]);
                                          Serial.print(ifs[(rt[j]&0b111)-1].pt.d[k], BIN);
                                          Serial.print(" ");
                                          }
                                      Serial.println();

                      ifs[(rt[j]&0b111)-1].pt.di=-13;
                              ifs[(rt[j]&0b111)-1].pt.time=micros();

                      ifs[(rt[j]&0b111)-1].pt.ds=1;
            }
          }
          Serial.println("Forward");
        }
      }
      ifs[i].pr.di=-1;
      ifs[i].pr.f=0;
      ifs[i].pr.a=0;
      ifs[i].pr.l=0;
      ifs[i].pr.d[0]=0;
    }
    
  }
    // if(millis()-perc>=PER){
    //   for(int i=0; i<1; i++){
    //     perc=millis();
    //     ifs[i].pt.time=micros();
    //     ifs[i].pt.ds=1;
    //   }
    // }
}