#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmsis_os2.h>
#include <lpc17xx.h>
#include "random.h"
#include "lfsr113.h"
#include "GLCD.h"
#include <string.h>
/*
		Final Lab Project 
		
		Group Member 1 : Akashdeep Singh Gill (20603396)
		Group Member 2 : Michael Cheng (20672092)

*/

//Global Variables

int pot; //potentiometer value
int system_state; //system state to switch between which tasks are running
int health; //keep track of health
int distance; //keep track of distance
int frequency; //changes frequency of obstacles appearing
int speed; // changes delay value based on level
char grid[10][20]; //grid array to store obstacles	
char grid_prev[10][20]; //grid array to store previous position of obstacles
int char_pos; //track character position
int char_prev; //track previous character position 
char ch[1]; //typecast to char for GLCD display

osMutexId_t mtx_start;

//initialzizing and creating mutex
const osMutexAttr_t Thread_Mutex_attr = {
  "myThreadMutex",
  osMutexRecursive | osMutexPrioInherit,
  NULL,
  0U
};

void CreateMutex (void){
  mtx_start = osMutexNew(&Thread_Mutex_attr);
}

//intializing global variables and peripherals
void init(){
	//Initialize globals
	pot = 0;
	system_state = 1;
	health = 3;
	distance = 0;
	frequency = 5;
	char_pos = 5; //intial position of user character
	char_prev = char_pos; //initialize prev position
	
	//Initiliaze grid to spaces
	for(int i=0;i<=19;i++){
		for(int j=0;j<=9;j++){	
			grid[j][i]=' ';
			grid_prev[j][i]=grid[j][i];
		}
	}
	
	//Intialize LCD and Potentiometer
	GLCD_Init();
	GLCD_Clear(0xffff);
	GLCD_SetBackColor(0xffff);
	GLCD_SetTextColor(0x0000);
	
	LPC_GPIO2->FIODIR |= 0x0000007C;
	LPC_GPIO1->FIODIR |= 0xB0000000;
	LPC_GPIO2->FIOCLR;
	LPC_GPIO2->FIOSET = 0x00000010;
	LPC_GPIO2->FIOSET = 0x00000020;
	LPC_GPIO2->FIOSET = 0x00000040;
	
	LPC_PINCON->PINSEL1 &= ~(0x03<<18);
	LPC_PINCON->PINSEL1 |= (0x01<<18);
	LPC_SC->PCONP |= (0x1<<12);
	
	LPC_ADC->ADCR = (1 << 2) |     // select AD0.2 pin          
									(4 << 8) |     // ADC clock is 25MHz/5          
									(1 << 21);     // enable ADC
}

//This thrread reads the value from the potentiometer and diplays it on the screen
void potentiometer(){
	//While system state is 1, ends when system state updates
	while (system_state==1){	
		LPC_ADC->ADCR |= (1 << 24);
		
		if((LPC_ADC->ADGDR & 0x80000000) == 0x80000000){
			pot = LPC_ADC->ADGDR &  0xfff0;
			pot = pot>>4;
			
			//Mutex to prevent errors with displaying on screen
			osMutexAcquire(mtx_start,osWaitForever);
			
			// Print level and update frequency depending on position of potentiometer
			if(system_state==1){
				if(pot<1366){
					GLCD_DisplayString(4, 6, 1, "Level 1");
					speed = 50;
				}
				else if(pot<2731){
					GLCD_DisplayString(4, 6, 1, "Level 2");
					speed = 150;
				}
				else{
					GLCD_DisplayString(4, 6, 1, "Level 3");
					speed = 0;
				}
			}
			osMutexRelease(mtx_start);
		}
	}
}

//This thread allows the user to select the displayed level and start the game 
void pushbutton(){
	//While system state is 1, ends when system state updates
	while(system_state==1){
		//If pushbutton is pressed
		if(((LPC_GPIO2->FIOPIN & 0X400) != 0x400)){
			//Mutex to prevent errors with displaying on screen
			osMutexAcquire(mtx_start,osWaitForever);
			GLCD_DisplayString(4, 6, 1, " Start!");
			osDelay(osKernelGetTickFreq());
			GLCD_Clear(0xffff);
			// Increase system state to move on to next batch of tasks
			system_state++;
			osMutexRelease(mtx_start);
		}		
	}
}

//This thread allows the user to move the user character in the screen to dodge obstacle characters
void joystick(){
	while(1){
		//Check system state
		if(system_state==2){
			osDelay((osKernelGetTickFreq())/10);//delay to control charachter movement
			                  
			if((LPC_GPIO1->FIOPIN & 0X800000) != 0x800000){
				if(char_pos!=0){
					char_pos--;
				}
			}			
			if((LPC_GPIO1->FIOPIN & 0X2000000) != 0x2000000){
				if(char_pos!=9){
					char_pos++;
				}
			}			
		}
	}
}

//This thread generates obstacle characters randomly, srolls the game environment, keeps tack of the distance covered 
//and prints the game environment on the screen

void obstacle_env(){
	char random;
	
	while(1){
		if(system_state==2){		
			//Delay depending on difficulty
			if(speed != 0){
				osDelay((osKernelGetTickFreq())/speed);
			}
			
			//Move obstacles down the grid		
			for(int i=1;i<=19;i++){
				for(int j=0;j<=9;j++){	
					grid[j][i-1]=grid[j][i];
				}
			}	
			//Set the top row to spaces
			for(int j=0;j<=9;j++){
				grid[j][19] = ' ';
			}
			//generate random character and set random row in top to spaces
			if(distance%frequency == 0){
				random = rand()%10;
				grid[random][19] = random+65;
			}
			
			//increase distance travelled
			distance++;
			
			osMutexAcquire(mtx_start,osWaitForever);
			// array to string and display strings
			GLCD_SetTextColor(0xf800);
			for(int i=19;i>=0;i--){
				for(int j=0;j<=9;j++){
					sprintf(ch,"%c", grid[j][i]);
					if(grid[j][i] != ' '){
						GLCD_DisplayString(j, i, 1, ch); //(char) grid[i][j]
					}
					if(grid_prev[j][i] != ' '){
						GLCD_DisplayString(j, i, 1, " "); //(char) grid[i][j]
					}
					grid_prev[j][i] = grid[j][i];
				}
			}
				
			//Print charachter position on screen 
			GLCD_SetTextColor(0x0000);
			GLCD_DisplayString(char_pos, 0, 1, "X");	
			if(char_prev != char_pos){		
				GLCD_DisplayString(char_prev, 0, 1, " ");
				char_prev = char_pos;
			}				
			osMutexRelease(mtx_start);
		}
	}
}


//This thread keeps a tack of any collision between the user character and obstacle characters, health of the user and displays the health on LEDs, 
//and prints the game over screen with the total distance covered
void collision(){
	int health_state=0;
	int prev_dist = 0;
	char distance_covered[10];
	int loop_control =1;
	while(loop_control){
		if(system_state==2){
			for(int j = 0;j<=9;j++){
				if(grid[j][0] != ' ' && char_pos == j && health_state == 0){
					health--;
					prev_dist = distance;
					health_state = 1;
				}
			}
			// Ensure distance has increased to prevent multiple health loss with same obstacle
			if(prev_dist != distance){
					health_state = 0;
			}
			
			//Update LEDs based on health
			if(health == 2){ 
				LPC_GPIO2->FIOCLR = 0x00000010;
			}
			else if(health == 1){
				LPC_GPIO2->FIOCLR = 0x00000010;
				LPC_GPIO2->FIOCLR = 0x00000020;
			}
			else if(health == 0){
				LPC_GPIO2->FIOCLR = 0x00000010;
				LPC_GPIO2->FIOCLR = 0x00000020;
				LPC_GPIO2->FIOCLR = 0x00000040;
				system_state++;
				loop_control=0;
			}
		}
		
	}
	osMutexAcquire(mtx_start,osWaitForever); // mutex to ensure no display errors
	
	//Display game over and distance covered
	GLCD_Clear(0xffff);
	GLCD_DisplayString(4, 4, 1, "GAME OVER!!!");
	GLCD_DisplayString(5, 1, 1, "Distance Covered:");
	sprintf(distance_covered,"%d", distance);
	GLCD_DisplayString(6, 9, 1, distance_covered);
	
	osMutexRelease(mtx_start);	
}

int main(){
	
	// run init function, initialize kernel, create mutex
	init();
	osKernelInitialize();
	CreateMutex();
	
	
	//Intialize threads
	osThreadNew(potentiometer, NULL, NULL);
	osThreadNew(pushbutton, NULL, NULL);
	osThreadNew(joystick, NULL, NULL);
	osThreadNew(obstacle_env, NULL, NULL);
	osThreadNew(collision, NULL, NULL);
	
	osKernelStart();
	return 0;
}
