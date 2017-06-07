#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <NewPing.h>
#include <VirtualWire.h>

#include "IMUsensor.hpp"
#include "PID.hpp"
#include "motormanager.hpp"

bool safe_mode = 1;             //si activé, les moteurs se coupent automatiquement après 3 secondes d'allumage
bool wait_serial = 0;                // et ce afin d'éviter une perte de contrôle du quadricoptère sur le banc de test
bool radio_debug = 1;
bool serial_debug = 0;

void setup()
{
  pinMode(9, INPUT_PULLUP); //on configure les entrées pour pouvoir utiliser le bouton
  Serial.begin(115200);

  vw_setup(1200);

}


void loop()
{
  if(wait_serial)
  {
    while(!Serial); //on attends que le port série soit ouvert pour commencer les calculs
  }

  bool motor_started = 0;
  unsigned long millis_at_motor_start = 0;
  unsigned long millis_at_last_print = 0;
  unsigned long millis_at_last_loop = 0;
  unsigned long time_loop = 0;


  NewPing sonar(6,5, 500);

  int sonar_height;

  IMUsensor mpu;                  //objet pour récupérer les valeurs de l'IMU et calculer une orientation absolue
  PID pid;                        //objet qui gère le calcul des directives pour les moteurs
  MotorManager motors;            //objet qui gère le calcul des valeurs par moteur, et s'occupe de les contrôler

  mpu.calibrateSensors();
  motors.startMotors();

  mpu.actualizeSensorData();
  mpu.calcAbsoluteOrientation(0.97);

  pid.calcCommand(mpu.getX(), mpu.getY(), mpu.getZ(), 0, mpu.getAngularSpeedX(), mpu.getAngularSpeedY(), mpu.getAngularSpeedZ(), 0, 0, 0, 10);



  delay(1000);

  while(true)
  {
    time_loop = millis() - millis_at_last_loop;
    millis_at_last_loop = millis();

    sonar_height = sonar.ping_cm();

    mpu.actualizeSensorData();
    mpu.calcAbsoluteOrientation(0.97);


    if( !digitalRead(9) == LOW )
    {
      pid.reset();
      motors.setOff();
      if(!safe_mode)
        motor_started = 0;
    }
    else
    {
      if(!motor_started)
      {
        motor_started = 1;
        millis_at_motor_start = millis();
        motors.setOn();
      }

      //calcul du PID avec les valeurs de l'IMU
      pid.calcCommand(mpu.getX(), mpu.getY(), mpu.getZ(), sonar_height + 10, mpu.getAngularSpeedX(), mpu.getAngularSpeedY(), mpu.getAngularSpeedZ(), 0, 0, 0, 10);
      float command_h = (pid.getCommandH() > 20) ? 20 : pid.getCommandH();
      motors.command( pid.getCommandX(), pid.getCommandY(), pid.getCommandZ(), command_h ); //commande des moteurs avec les valeurs données par le PID

    }

    if(millis() - millis_at_last_print > 30)
    {
      if(radio_debug)
      {
        uint8_t message[7];
        message[0] = motors.getMotorValue(0);
        message[1] = motors.getMotorValue(1);
        message[2] = motors.getMotorValue(2);
        message[3] = motors.getMotorValue(3);
        message[4] = mpu.getX() + 127;
        message[5] = mpu.getY() + 127;
        message[6] = millis() / 1000;

        vw_send((uint8_t *)message, 7);

      }
      if(serial_debug)
      {
        Serial.print( motors.getMotorValue(0) );  Serial.print("\t");
        Serial.print( motors.getMotorValue(1) );  Serial.print("\t");
        Serial.print( motors.getMotorValue(2) );  Serial.print("\t");
        Serial.print( motors.getMotorValue(3) ); Serial.print("\t|\t");
        Serial.print( mpu.getX(), 2 ); Serial.print("\t");
        Serial.print( mpu.getY(), 2 ); Serial.print("\t|\t");
        Serial.print( sonar_height , DEC) ; Serial.print("\t | \t");
        Serial.print( pid.getCommandH() ); Serial.print("\n");

      }
      millis_at_last_print = millis();
    }

    if ( safe_mode && motor_started  && millis() - millis_at_motor_start > 3000 )
    {
      motors.setOff();
      Serial.println("stop bc of millis > 3000");
    }
  }
}
