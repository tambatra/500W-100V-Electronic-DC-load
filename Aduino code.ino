#include <Wire.h>
#include "DAC_MCP4725.h"
#include <MsTimer2.h>
#include <ButtonDebounce.h>

ButtonDebounce btYES(6, 100);
ButtonDebounce btNO(5, 100);
ButtonDebounce btSTOP(7, 100);
#define RL 8
#define FAN 9
#define SETTING A3
#define GAINSW 3

DAC_MCP4725 dac;

unsigned long lastMillis = 0;

float temperature = 0.0;
float temperatureTemp = 0.0;
float current = 0.0;
float voltage = 0.0;
float voltageTemp = 0.0;
float currentTemp = 0.0;
float battCuttOffvoltage = 0.0;
float panelCuttOffvoltage = 0.0;
float x = 0.0;
float battCapacity = 0.0;
float xTemp = 0.0;
unsigned int dacCtrl = 0;
unsigned int dacCtrlTemp = 0;
byte mode = 0;
byte fanSpeed = 0;
byte fanSpeedTemp = 0;
boolean btYesPushed = false;
boolean btNoPushed = false;
boolean btStopPushed = false;
boolean battTest = false;
boolean panelCutOffSet = false;

void updateCriticalParamsMoy(){
	static byte countParams = 0;
	countParams++;
	voltageTemp += (float)analogRead(A0); 
	currentTemp += (float)analogRead(A1);
	if(countParams == 5){
		voltageTemp = voltageTemp/5.0;
		currentTemp = currentTemp/5.0;
		voltage = voltageTemp*0.0536;
		if(digitalRead(GAINSW) == HIGH){
		    current = currentTemp * 0.004803; // GAIN 1
		    //current = currentTemp * 0.026838; // GAIN 1
		}
		else{
			current = currentTemp * 0.026986; // GAIN 2
			//current = currentTemp * 0.026838; // GAIN 2
		}
		countParams = 0;
		voltageTemp = 0.0;
		currentTemp = 0.0;
	}
	if(dacCtrlTemp != dacCtrl){
		dac.setVoltage(dacCtrl, false);
		dacCtrlTemp = dacCtrl;
	}
	fanCtrl();
}

void updateCriticalParams(){
	voltage = (float)analogRead(A0); 
	voltage *= 0.0536;
	current = (float)analogRead(A1);
	if(digitalRead(GAINSW) == HIGH){
	    current *= 0.004803; // GAIN 1
	}
	else{
		current *= 0.026986; // GAIN 2
	}
	if(dacCtrlTemp != dacCtrl){
		dac.setVoltage(dacCtrl, false);
		dacCtrlTemp = dacCtrl;
	}
	fanCtrl();
}

void updateTemperature(){
	static byte count = 0;
	temperatureTemp += (float)analogRead(A2);
	count++;
	if(count==20){
	    temperature = (temperatureTemp/20.0)* 0.488;;
	    temperatureTemp = 0.0;
	    count = 0;
	}
}

void fanCtrl(){
	if(temperature<35.0){
		fanSpeed = 0;
	}
	else{
		if((temperature * 3.5)>255) fanSpeed = 255;
		else fanSpeed = (byte)(temperature * 3.5);
	}
	if(fanSpeedTemp != fanSpeed){
		analogWrite(FAN, fanSpeed);
		fanSpeedTemp = fanSpeed;
	}
}

void relayCTRL(){
	if(voltage>0.2){
		digitalWrite(RL, HIGH);
		Serial.println("Relay ON");
	} 
	else {
		digitalWrite(RL, LOW);
		Serial.println("INPUT VOLATGE ERROR, Relay OFF");
	}
}

void updateDisplay(){
	Serial.print("IS: ");
	Serial.print(current, 2);
	Serial.print("  VS: ");
	Serial.print(voltage, 1);
	Serial.print("  PWR: ");
	Serial.print(voltage*current, 1);
	Serial.print("  DAC: ");
	Serial.print(dacCtrl);
	Serial.print("  GAINSW: ");
	Serial.print(digitalRead(GAINSW));
	Serial.print("  IN: ");
	Serial.print(analogRead(A1));
	if(!battTest){
		Serial.print("  TEMP: ");
		Serial.println(temperature, 1);
	}
	else{
	    Serial.print("  CAPACITY: ");
		battCapacity += (current/3.6);
		Serial.print(battCapacity,1);
	    Serial.println("mAh");
	}
}


void btUpdate(){
	btYES.update();
	btNO.update();
	btSTOP.update();
}


void waitYESHIGH(){
	delay(50);
	while(btYES.state() == HIGH){btUpdate();}
	delay(50);
	btUpdate();
}

void waitNOHIGH(){
	delay(50);
	while(btNO.state() == HIGH){btUpdate();}
	delay(50);
	btUpdate();
}

void waitSTOPHIGH(){
	delay(50);
	while(btSTOP.state() == HIGH){btUpdate();}
	delay(50);
	btUpdate();
}

// SETUP
void setup(){
	Serial.begin(9600);
	dac.begin(0x62);
	dac.setVoltage(0, false);
	pinMode(RL, OUTPUT);
	pinMode(FAN, OUTPUT);
	pinMode(7, INPUT_PULLUP);
	pinMode(6, INPUT_PULLUP);
	pinMode(5, INPUT_PULLUP);
	delay(1000);
}

void loop(){
	mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
	updateCriticalParams();
	//Constant Current Mode
	if(mode == 18){
		Serial.println("CC MODE");
		Serial.println("SET CURRENT (A): ");
		while(mode == 18){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			x = (float)analogRead(SETTING)/50.0;
			if (millis() - lastMillis > 500){
				lastMillis = millis();
				if(xTemp!=x){
					xTemp = x;
					Serial.println(x, 2);
				}
			}
		    if(btYES.state() == HIGH){
		    	waitYESHIGH();
		    	Serial.print("START CC MODE @ : ");
				Serial.print(x,2);
				Serial.println("A ?");
				while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
					waitYESHIGH();
					Serial.println("CC MODE STARTED...");
					updateCriticalParams();
					relayCTRL();
					while(btSTOP.state()==LOW && voltage>0.2){
						btUpdate();
						//dacCtrl = (int)(x*299.0);
						if(current<x) dacCtrl += 1;
						else dacCtrl -= 1;
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							updateDisplay();
							updateTemperature();
						}
					}
					waitSTOPHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CC MODE STOPPED");
					Serial.println("CC MODE");
					Serial.println("SET CURRENT (A): ");
					x = (float)analogRead(SETTING)/50.0;
					Serial.println(x,2);
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CC MODE CANCELLED!");
					Serial.println("SET CURRENT (A): ");
					x = (float)analogRead(SETTING)/50.0;
					Serial.println(x,2);
				}
			}
			if(btNO.state()==HIGH){
				waitNOHIGH();
				dacCtrl = 0;
				updateCriticalParams();
				return;
			}
		}
	}


	//Constant VOLATGE Mode
	if(mode == 17){
		Serial.println("CV MODE");
		Serial.println("SET VOLTAGE (V): ");
		while(mode == 17){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			x = (float)analogRead(SETTING)/20.0;
			if (millis() - lastMillis > 500){
				lastMillis = millis();
				if(xTemp!=x){
					xTemp = x;
					Serial.println(x, 1);
				}
			}
		    if(btYES.state() == HIGH){
		    	waitYESHIGH();
		    	Serial.print("START CV MODE @ : ");
				Serial.print(x,1);
				Serial.println("V ?");
				while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
					waitYESHIGH();
					Serial.println("CV MODE STARTED...");
					updateCriticalParams();
					relayCTRL();
					while(btSTOP.state()==LOW && voltage>(x-1.0)){
						btUpdate();
						if(voltage>x) dacCtrl += 1;
						else dacCtrl -= 1; 
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							updateDisplay();
							updateTemperature();
						}
					}
					waitSTOPHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CV DONE\n");
					Serial.println("CV MODE");
					Serial.println("SET VOLTAGE (V): ");
					x = (float)analogRead(SETTING)/20.0;
					Serial.println(x,1);
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CV MODE CANCELLED!");
					Serial.println("SET VOLTAGE (V): ");
					x = (float)analogRead(SETTING)/20.0;
					Serial.println(x,1);
				}
			}
			if(btNO.state()==HIGH){
				waitNOHIGH();
				dacCtrl = 0;
				updateCriticalParams();
				return;
			}
		}
	}


	//Constant RESISTANCE Mode
	if(mode == 15){
		Serial.println("CR MODE");
		Serial.println("SET RESISTANCE (Ohm): ");
		while(mode == 15){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			x = (float)analogRead(SETTING)/2.0;
			if (millis() - lastMillis > 500){
				lastMillis = millis();
				if(xTemp!=x){
					xTemp = x;
					Serial.println(x,1);
				}
			}
		    if(btYES.state() == HIGH){
		    	waitYESHIGH();
		    	Serial.print("START CR MODE @ : ");
				Serial.print(x,1);
				Serial.println("Ohm ?");
				while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
					waitYESHIGH();
					Serial.println("CR MODE STARTED...");
					updateCriticalParams();
					relayCTRL();
					while(btSTOP.state()==LOW && voltage>0.2){
						btUpdate();
						//dacCtrl = (int)((voltage/x)*295.3);
						if(voltage/current>x) dacCtrl += 1;
						else dacCtrl -= 1;
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							updateDisplay();
							updateTemperature();
						}
					}
					waitSTOPHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CR MODE STOPPED");
					Serial.println("CR MODE");
					Serial.println("SET RESISTANCE (Ohm): ");
					x = (float)analogRead(SETTING)/2.0;
					Serial.println(x,1);
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CR MODE CANCELLED!");
					Serial.println("SET RESISTANCE (Ohm): ");
					x = (float)analogRead(SETTING)/2.0;
					Serial.println(x,1);
				}
			}
			if(btNO.state()==HIGH){
				waitNOHIGH();
				dacCtrl = 0;
				updateCriticalParams();
				return;
			}
		}
	}


	//Constant POWER Mode
	if(mode == 8){
		Serial.println("CP MODE");
		Serial.println("SET POWER (WATT): ");
		while(mode == 8){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			x = (float)analogRead(SETTING)/2.0;
			if (millis() - lastMillis > 500){
				lastMillis = millis();
				if(xTemp!=x){
					xTemp = x;
					Serial.println(x,1);
				}
			}
		    if(btYES.state() == HIGH){
		    	waitYESHIGH();
		    	Serial.print("START CP MODE @ : ");
				Serial.print(x);
				Serial.println("WATT ?");
				while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
					waitYESHIGH();
					Serial.println("CP MODE STARTED...");
					updateCriticalParams();
					relayCTRL();
					while(btSTOP.state()==LOW && voltage>0.2){
						btUpdate();
						//dacCtrl = (int)((x/voltage)*295.3);
						if(voltage*current<x) dacCtrl += 1;
						else dacCtrl -= 1;
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							updateDisplay();
							updateTemperature();
						}
					}
					waitSTOPHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CP MODE STOPPED");
					Serial.println("CP MODE");
					Serial.println("SET POWER (WATT): ");
					x = (float)analogRead(SETTING)/2.0;
					Serial.println(x,1);
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("CP MODE CANCELLED!");
					Serial.println("SET POWER (WATT): ");
					x = (float)analogRead(SETTING)/2.0;
					Serial.println(x,1);
				}
			}
			if(btNO.state()==HIGH){
				waitNOHIGH();
				dacCtrl = 0;
				updateCriticalParams();
				return;
			}
		}
	}


	//BATTERY CAPACITY TEST MODE
	if(mode == 4){
		Serial.println("BATTERY MODE");
		Serial.println("SET LOAD CURRENT (A): ");
		while(mode == 4){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			x = (float)analogRead(SETTING)/50.0;
			if (millis() - lastMillis > 500){
				lastMillis = millis();
				if(xTemp!=x){
					xTemp = x;
					Serial.println(x,2);
				}
			}
		    if(btYES.state()==HIGH){
		    	waitYESHIGH();
		    	Serial.println("SET BATT CUTOFF VOLTAGE (V): ");
				while(btYES.state()==LOW){
					btUpdate();
					battCuttOffvoltage = ((float)analogRead(SETTING))/20.0;
					if (millis() - lastMillis > 500){
						lastMillis = millis();
						if(xTemp!=battCuttOffvoltage){
							xTemp = battCuttOffvoltage;
							Serial.println(battCuttOffvoltage,1);
						}
					}
				}
				waitYESHIGH();
		    	Serial.println("START BATT CAPACITY TEST?");
		    	Serial.print("CURRENT : ");
		    	Serial.print(x,2);
		    	Serial.print("A    CUTOFF VOLT: ");
		    	Serial.print(battCuttOffvoltage,1);
		    	Serial.println("V");
		    	while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
			    	waitYESHIGH();
					Serial.println("BATT TEST STARTED...");
					updateCriticalParams();
					relayCTRL();
					battTest = true;
					battCapacity = 0.0;
					while(btSTOP.state()==LOW && voltage>battCuttOffvoltage){
						//dacCtrl = (int)(x*299.4);
						if(current<x) dacCtrl += 1;
						else dacCtrl -= 1;
						btUpdate();
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							updateDisplay();
							updateTemperature();
						}
					}
					waitSTOPHIGH();
					battTest = false;
					dacCtrl = 0;
					updateCriticalParams();
					Serial.print("BATT CAPACITY: ");
					Serial.print(battCapacity);
					Serial.println("mAh");
					while(btYES.state()==LOW){btUpdate();}
					waitYESHIGH();
					Serial.println("BATTERY MODE");
					Serial.println("SET LOAD CURRENT (A): ");
					x = (float)analogRead(SETTING)/50.0;
					Serial.println(x,2);
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("BATT TEST CANCELLED!");
				}
			}
			if(btNO.state()==HIGH){
		    	waitNOHIGH();
				dacCtrl = 0;
				updateCriticalParams();
				return;
			}
		}
	}


	//SOLAR PANEL PARAMETERS TEST Mode
	if(mode == 1){
		Serial.println("PANEL MODE");
		Serial.println("SET CUTOFF VOLT?");
		while(mode == 1){
			updateTemperature();
			updateCriticalParams();
			btUpdate();
			mode = (byte)map(analogRead(A6), 0, 1023, 0, 20);
			panelCutOffSet = false;
			if(btYES.state()==HIGH){
				waitYESHIGH();
				Serial.print("SET CUTOFF VOLTAGE: ");
				while(btYES.state()==LOW){
					btUpdate();
					if(btNO.state()==HIGH){
						waitNOHIGH();
					    return;
					}
					panelCuttOffvoltage = ((float)analogRead(SETTING))/50.0;
					if (millis() - lastMillis > 500){
						lastMillis = millis();
						if(xTemp!=panelCuttOffvoltage){
							xTemp = panelCuttOffvoltage;
							Serial.println(panelCuttOffvoltage,2);
						}
					}
				}
				waitYESHIGH();
				panelCutOffSet = true;
			}
			if(btNO.state()==HIGH){
		    	waitNOHIGH();
				Serial.println("CUTOFF VOLTAGE is DEFALUT TO 0V");
				panelCuttOffvoltage = 0;
				panelCutOffSet = true;
			}

			if(panelCutOffSet){
				Serial.println("START PANEL TEST?");
				Serial.print("CUTOFF VOLTAGE: ");
				Serial.print(panelCuttOffvoltage,2);
				Serial.println(" V");
				while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
				if(btYES.state()==HIGH){
					waitYESHIGH();
					dacCtrl = 0;
					Serial.println("PANEL TEST STARTED!");
					float pmax = 0.0;
					float vpmax = 0.0;
					float ipmax = 0.0;
					float voc = 0.0;
					float icc = 0.0;
					updateCriticalParams();
					relayCTRL();
					while(btSTOP.state()==LOW && voltage>panelCuttOffvoltage){
						dacCtrl = dacCtrl + 1;
						//delay(10);
						btUpdate();
						updateCriticalParams();
						if (millis() - lastMillis > 1000){
							lastMillis = millis();
							//updateDisplay();
							updateTemperature();
						}
						float p = voltage*current;
						if(voc<voltage){voc = voltage;}
						if(icc<current){icc = current;}
						if(pmax<p){
							pmax = p;
							vpmax = voltage;
							ipmax = current;
						}
						
					}
					waitSTOPHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					Serial.println("PANEL TEST DONE!");
					Serial.print("Pmax = ");
					Serial.print(pmax);
					Serial.print(" Watt   ");
					Serial.print("Vpmax = ");
					Serial.print(vpmax);
					Serial.print(" V   ");
					Serial.print("Ipmax = ");
					Serial.print(ipmax);
					Serial.print(" A   ");
					Serial.print("Voc = ");
					Serial.print(voc);
					Serial.print(" V   ");
					Serial.print("Icc = ");
					Serial.print(icc);
					Serial.println(" A   ");
					Serial.println("CLICK YES TO CONTINUE/ NO TO RESTART");
					while(btYES.state()==LOW && btNO.state()==LOW){btUpdate();}
					if(btNO.state() == HIGH){
						waitNOHIGH();
						return;
					}
				}
				else{
					waitNOHIGH();
					dacCtrl = 0;
					updateCriticalParams();
					updateTemperature();
					Serial.println("PANEL TEST STOPPED!");
					Serial.println("SET CUTOFF VOLT?");
				}
			}
		}
	}

}


