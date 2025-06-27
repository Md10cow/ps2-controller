// Константы
const short pinClockMain = 5;
const short pinDataMain = 6;
const short pinClockKB = 3;
const short pinDataKB = 4;
const short pinClockMS = 5;
const short pinDataMS = 6;
const short pinPower = 2;
const uint16_t bufLenMSIn = 512;
const uint16_t bufLenKBIn = 256;
const uint16_t bufLenMSOut = 16;
const uint16_t bufLenKBOut = 16;
const uint16_t bufLenMainIn = 16;
const uint16_t bufLenMainOut = 16;

// Глобальные переменные
volatile unsigned char MSsym = 0;
volatile unsigned char MSparF = 0;
volatile unsigned char MScount = 0;
volatile unsigned char KBsym = 0;
volatile unsigned char KBparF = 0;
volatile unsigned char KBcount = 0;
volatile unsigned short chr;
volatile bool MSbl = 0;
volatile bool KBbl = 0;
volatile uint8_t oldD = 0;
volatile uint8_t oldB = 0;
volatile uint8_t oldC = 0;
unsigned char bufferMS[bufLenMSIn];
unsigned char bufferKB[bufLenKBIn];
short MSRead = 0;
short MSWrite = 0;
short KBRead = 0;
short KBWrite = 0;
bool KBIOF = 0;
bool KBOOF = 0;
bool MSIOF = 0;
bool MSOOF = 0;
bool MnIOF = 0;
bool MnOOF = 0;
volatile bool curClock = 0;
unsigned int oldTime = 0;
unsigned int oldTimeKB = 0;
unsigned int oldTimeMS = 0;
volatile short countMain = 0;
volatile uint8_t parFMain = 1;
volatile uint8_t symMain = 0;
uint8_t maskMain = 1;
volatile uint8_t maskKB = 1;
volatile uint8_t maskMS = 1;
volatile bool Mainbl = 1;
unsigned char bufferKBOut[bufLenKBOut];
unsigned char bufferMSOut[bufLenMSOut];
short MSORead = 0;
short MSOWrite = 0;
short KBORead = 0;
short KBOWrite = 0;
bool KBmode = 1;
bool MSmode = 1;
bool KBstart = 0;
bool MSstart = 0;
bool mainMode = 1;
bool mainStart = 0;
unsigned char bufferMainIn[bufLenMainIn];
unsigned char bufferMainOut[bufLenMainOut];
short MainIRead = 0;
short MainIWrite = 0;
short MainORead = 0;
short MainOWrite = 0;
short curPort = -1;
bool QF = 0;

// Быстрый digitalRead для опроса внутри ISR
bool pinRead(uint8_t pin) {
  if (pin < 8) {
    return bitRead(PIND, pin);
  } else if (pin < 14) {
    return bitRead(PINB, pin - 8);
  } else if (pin < 20) {
    return bitRead(PINC, pin - 14);
  }
}

// Чтение с устройства
void readPS2(int data
              , volatile unsigned char &sym
              , volatile unsigned char &parF
              , volatile unsigned char &count
              , volatile bool &bl) 
              {
                chr = pinRead(data);
                if (count == 0) {
                  parF = 0;
                  sym = 0;
                  bl = 0;
                  if (chr != 0) {
                    return;
                  }
                    count++;
                }
                else if (count > 0 && count < 9) {
                  chr = pinRead(data);
                  parF ^= chr;
                  count++;
                  sym = sym | (chr << (count - 2));
                  return;
                }
                else if (count == 9) {
                    chr = pinRead(data);
                    count++;
                    if (chr == parF) {
                      count = 0;
                      parF = 0;
                      sym = 0;
                      bl = 0;

                      return;
                    }
                }
                else if (count == 10) {
                      chr = pinRead(data);
                      count = 0;
                      parF = 0;
                      bl = chr == 1;
                      return;
    }
}

// Чтение с хоста
void readMain(int data) {
  chr = pinRead(data);
  if (countMain == 0) {
    parFMain = 0;
    symMain = 0;
    Mainbl = 0;
    if (chr != 0) {
      return;
    }
      countMain++;
  }
  else if (countMain > 0 && countMain < 9) {
    chr = pinRead(data);
    parFMain ^= chr;
    countMain++;
    symMain = symMain | (chr << (countMain - 2));
    return;
  }
  else if (countMain == 9) {
      chr = pinRead(data);
      countMain++;
      if (chr == parFMain) {
        countMain = 0;
        parFMain = 0;
        symMain = 0;
        Mainbl = 0;

        return;
      }
  }
  else if (countMain == 10) {
        chr = pinRead(data);
        countMain = 0;
        parFMain = 0;
        Mainbl = chr == 1;
        return;
    }
}

// Запись на устройство
void printPS2(int data
              , volatile unsigned char &sym
              , volatile unsigned char &parF
              , volatile unsigned char &count
              , volatile bool &bl
              , volatile uint8_t&  mask) {

  if (count <= 8 && count > 0) {
      uint8_t tmp = (sym & mask) >> (count - 1);
      digitalWrite(data, tmp);
      parF ^= tmp;
      mask <<= 1;
      count++;
  }

  else if (count == 9) {
      digitalWrite(data, parF);
      count++;
  }

  else if (count == 10) {
      digitalWrite(data, 1);
      count = 11;
  }
}

// Прерывание PCINT2
ISR(PCINT2_vect) {  // пины 0-7
  uint8_t pinF = (PIND ^ oldD) & oldD; // Изменения с 1 в 0
  uint8_t pinR = (PIND ^ oldD) & PIND; // Изменения с 0 в 1
  
  // Инициализация чтения с хоста
  if (mainMode && curClock && bitRead(pinF, pinClockMain)) {
      pinMode(pinDataMain, INPUT);
      countMain = 0;
      curClock = 0;
      mainMode = 0;
  }

  // Чтение с клавиатуры
  if (KBmode && bitRead(pinF, pinClockKB)) {

    readPS2(pinDataKB, KBsym, KBparF, KBcount, KBbl);
  }

  // Запись на клавиатуру
  if (!KBmode) {
    if (!KBbl && bitRead(pinR, pinClockKB)) {
      printPS2(pinDataKB, KBsym, KBparF, KBcount, KBbl, maskKB);
      }

    else if ((KBcount == 11) && bitRead(pinF, pinClockKB)) {
      KBcount = 12;
      if (!pinRead(pinDataKB)) {
        KBbl = 1;
      }
    }
  }

  // Чтение с мыши
  if (MSmode && bitRead(pinF, pinClockMS)) {

    readPS2(pinDataMS, MSsym, MSparF, MScount, MSbl);
  }

  // Запись на мышь
  if (!MSmode) {
    if (!MSbl && bitRead(pinR, pinClockMS)) {
      printPS2(pinDataMS, MSsym, MSparF, MScount, MSbl, maskMS);
      }

    else if ((MScount == 11) && bitRead(pinF, pinClockMS)) {
      MScount = 12;
      if (!pinRead(pinDataMS)) {
        MSbl = 1;
      }
    }
  }

  oldD = PIND;
}

// функция для настройки PCINT
uint8_t attachPCINT(uint8_t pin) {
  if (pin < 8) {            // D0-D7 (PCINT2)
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << pin); return 2; } else if (pin > 13) {    //A0-A5 (PCINT1)
    PCICR |= (1 << PCIE1);
    PCMSK1 |= (1 << pin - 14);
    return 1;
  } else {                  // D8-D13 (PCINT0)
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << pin - 8);
    return 0;
  }
}

// Запись на хост
void setBit() {
  if (!countMain) {
    digitalWrite(pinDataMain, 0);
    countMain++;
    parFMain = 1;
    maskMain = 1;
  }
  else if (countMain <= 8) {
      uint8_t tmp = (symMain & maskMain) >> (countMain - 1);
      digitalWrite(pinDataMain, tmp);
      parFMain ^= tmp;
      maskMain <<= 1;
      countMain++;
  }

  else if (countMain == 9) {
      digitalWrite(pinDataMain, parFMain);
      countMain++;
  }

  else if (countMain == 10) {
      digitalWrite(pinDataMain, 1);
      countMain = 0;
      Mainbl = 1;
  }
}



void setup() {
  // put your setup code here, to run once:
    attachPCINT(pinClockKB);
    attachPCINT(pinClockMain);
    attachPCINT(pinClockMS);
    pinMode(pinClockKB, INPUT);
    pinMode(pinDataKB, INPUT);
    pinMode(pinClockMS, INPUT);
    pinMode(pinDataMS, INPUT);
    pinMode(pinPower, OUTPUT);
    digitalWrite(pinPower, 1);
    pinMode(pinClockMain, OUTPUT);
    digitalWrite(pinClockMain, 0);
    pinMode(pinDataMain, OUTPUT);
    digitalWrite(pinDataMain, 0);
    Serial.begin(9600);
  
}

void loop() {
  // Завершение отправки на хост
  if (Mainbl && mainMode) {
    MainORead++;
    MainORead &= bufLenMainOut - 1;
    mainStart = 0;
    Mainbl = 0;
  }

  // Начало отправки на хост
  if (mainMode && !mainStart && (MainORead != MainOWrite)) {
      symMain = bufferMainOut[MainORead];
      Mainbl = 0;
      countMain = 0;
      mainStart = 1;
  }
  
  // Генерация clock на хост и работа с data
  if ((micros() > oldTime) && (micros() - oldTime >= 50) || (micros() < oldTime) && (micros() >= 49 - (0xffffffff - oldTime))) {
      curClock ^= 1;
      if (!curClock) {
        pinMode(pinClockMain, OUTPUT);
        digitalWrite(pinClockMain, 0);
        if (!mainMode && !Mainbl) {
          readMain(pinDataMain);
        }
      }

      else {
        pinMode(pinClockMain, INPUT_PULLUP);
        if (mainMode && mainStart) {
          setBit();
        }

        if (!mainMode && Mainbl) {
            bufferMainIn[MainIWrite++] = symMain;
            if (MainIWrite & bufLenMainIn) {
              MnIOF = 1;
              MainIWrite = 0;
            }
            if (MnIOF && (MainIWrite == MainIRead)) {
              MainIRead++;
              MainIRead &= bufLenMainIn - 1;
            }
            Mainbl = 0;
            mainMode = 1;
            pinMode(pinDataMain, OUTPUT);
            digitalWrite(pinDataMain, 0);
        }
      }
      oldTime = micros();
  }

  // Отправка на клавиатуру
  if (KBORead != KBOWrite) {
    if (KBmode) {
      KBparF = 1;
      KBsym = bufferKBOut[KBORead];
      KBcount = 0;
      maskKB = 1;
      KBbl = 0;
      KBmode = 0;
      KBstart = 1;
      pinMode(pinDataKB, OUTPUT);
      pinMode(pinClockKB, OUTPUT);
      digitalWrite(pinClockKB, 0);
      oldTimeKB = micros();
      
    }

    else if (KBstart && ((micros() > oldTimeKB) && (micros() - oldTimeKB >= 100) || (micros() < oldTimeKB) && (micros() >= 99 - (0xffffffff - oldTimeKB)))) {
        digitalWrite(pinDataKB, 0);
        pinMode(pinClockKB, INPUT_PULLUP);
        KBstart = 0;
        KBcount++;
    }

    else if (KBcount == 12) {
        if (KBbl) {
          KBORead++;
          KBORead &= bufLenKBOut - 1;
        }

        pinMode(pinDataKB, INPUT);
        pinMode(pinClockKB, INPUT);
        KBbl = 0;
        KBcount = 0;
        KBmode = 1;
    }
  }

  // Отправка на мышь
  if (MSORead != MSOWrite) {
    if (MSmode) {
      MSparF = 1;
      MSsym = bufferMSOut[MSORead];
      MScount = 0;
      maskMS = 1;
      MSbl = 0;
      MSmode = 0;
      MSstart = 1;
      pinMode(pinDataMS, OUTPUT);
      pinMode(pinClockMS, OUTPUT);
      digitalWrite(pinClockMS, 0);
      oldTimeMS = micros();
      
    }

    else if (MSstart && ((micros() > oldTimeMS) && (micros() - oldTimeMS >= 100) || (micros() < oldTimeMS) && (micros() >= 99 - (0xffffffff - oldTimeMS)))) {
        digitalWrite(pinDataMS, 0);
        pinMode(pinClockMS, INPUT_PULLUP);
        MSstart = 0;
        MScount++;
    }

    else if (MScount == 12) {
        if (MSbl) {
          MSORead++;
          MSORead &= bufLenMSOut - 1;
        }

        pinMode(pinDataMS, INPUT);
        pinMode(pinClockMS, INPUT);
        MSbl = 0;
        MScount = 0;
        MSmode = 1;
    }
  }

  // Запись символа с мыши в буфер
   if (MSbl && MSmode) {
    bufferMS[MSWrite++] = MSsym;
    if (MSWrite & bufLenMSIn) {
       MSIOF = 1;
       MSWrite = 0;
    }
    if (MSIOF && (MSWrite == MSRead)) {
      MSRead++;
      
      MSRead &= bufLenMSIn - 1;
    }
    MSbl = 0;
    MSsym = 0;
  }

  // Запись символа с клавиатуры в буфер
  if (KBbl && KBmode) {
    bufferKB[KBWrite++] = KBsym;
    if (KBWrite & bufLenKBIn) {
       KBIOF = 1;
       KBWrite = 0;
    }
    if (KBIOF && (KBWrite == KBRead)) {
      KBRead++;
      KBRead &= bufLenKBIn - 1;
    }
    KBbl = 0;
    KBsym = 0;
  }

  // Основная логика
  if (MainIRead != MainIWrite) {
      uint8_t tmp = bufferMainIn[MainIRead++];
      MainIRead &= bufLenMainIn - 1;
      QF = tmp & 128;
      curPort = tmp & 127;
      if (QF) {
        switch (curPort) { // Обработка запроса с устройства
        case 1 : // Клавиатура
          bufferMainOut[MainOWrite++] = bufferKB[KBRead++];
          KBRead &= bufLenKBIn - 1;
          if (MainOWrite & bufLenMainOut) {
            MnOOF = 1;
            MainOWrite = 0;
          }
          if (MnOOF && (MainOWrite == MainORead)) {
            MainORead++;
            MainORead &= bufLenMainOut - 1;
          }
          break;
        
        case 2 : // Мышь
          bufferMainOut[MainOWrite++] = bufferMS[MSRead++];
          MSRead &= bufLenMSIn - 1;
          if (MainOWrite & bufLenMainOut) {
            MnOOF = 1;
            MainOWrite = 0;
          }
          if (MnOOF && (MainOWrite == MainORead)) {
            MainORead++;
            MainORead &= bufLenMainOut - 1;
          }
          break;
        }
      }

      else if (MainIRead != MainIWrite) { 
        switch (curPort) { // Пересылка пакета
        case 1 : // Клавиатура
          bufferKBOut[KBOWrite++] = bufferMainIn[MainIRead++];
          MainIRead &= bufLenMainIn - 1;
          if (KBOWrite & bufLenKBOut) {
            KBOOF = 1;
            KBOWrite = 0;
          }
          if (KBOOF && (KBOWrite == KBORead)) {
            KBORead++;
            KBORead &= bufLenKBOut - 1;
          }
          break;

        case 2 : // Мышь
          bufferMSOut[MSOWrite++] = bufferMainIn[MainIRead++];
          MainIRead &= bufLenMainIn - 1;
          if (MSOWrite & bufLenKBOut) {
            MSOOF = 1;
            MSOWrite = 0;
          }
          if (MSOOF && (MSOWrite == MSORead)) {
            MSORead++;
            MSORead &= bufLenMSOut - 1;
          }
          break;
        }
      }

      else if (MainIRead) {
        MainIRead--;
      }

      else {
        MainIRead = bufLenMainIn - 1;
      }
  }
}