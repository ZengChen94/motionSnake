/*
 * snakeV2.c
 *
 *  Created on: 2015-12-15
 *      Author: yifei
 */

#include <stdlib.h>

#include "xtft.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xintc.h"
#include "ADXL362.h"
#include "xil_cache.h"
#include "spi.h"

int rxData = 0;
void uartIntHandler(XIntc * InstancePtr)
{
	if(Xil_In32(XPAR_RS232_BASEADDR + 0x08) & 0x01)
	{
		rxData = Xil_In32(XPAR_RS232_BASEADDR);
	}
	/* Assert that the pointer to the instance is valid
	 */
	Xil_AssertVoid(InstancePtr != NULL);

	/* Use the instance's device ID to call the main interrupt handler.
	 * (the casts are to avoid a compiler warning)
	 */
	XIntc_DeviceInterruptHandler((void *)
					 ((u32) (InstancePtr->CfgPtr->DeviceId)));
}

#define TFT_DEVICE_ID XPAR_TFT_0_DEVICE_ID
#define TFT_FRAME_ADDR0		XPAR_XPS_MCH_EMC_0_MEM0_HIGHADDR - 0x001FFFFF
#define FGCOLOR_grn 0x00001c00
#define FGCOLOR_blu 0x0000000c
#define FGCOLOR_red 0x001c0000
#define N 200
#define ySIZE 2
#define xSIZE  10
#define xSTART 0
#define xEND 59
#define x_length 58
#define ySTART 0
#define yEND 45
#define y_length 44
#define xoffset 0
u32 colorSet[4]={0xFF,0xFF00,0xFF0000,0xFFFFFF};
int tempx,tempy;
int score = 0;
struct Food {
    int x;/*ʳ��ĺ�����*/
    int y;/*ʳ���������*/
    int yes;/*�ж��Ƿ�Ҫ����ʳ��ı���*/
}food;/*ʳ��Ľṹ��*/
struct Snake {
    int x[N];
    int y[N];
    int node;/*�ߵĽ���*/
    int direction;/*���ƶ�����*/
    int life;/* �ߵ�����,0����,1����*/
}snake;

static XTft TftInstance;
XTft_Config *TftConfigPtr;

int gamespeed=50000;/*��Ϸ�ٶ��Լ�����*/
void DrawRectangle(int x,int y,int color);
void DrawK(void);/*��ʼ����*/
void Initialize();
void Delay(int speed);
void GameOver();
void drawchar(int xPos,int yPos,char VarChar,int ratio);
void PlaySnake(void);/*����Ϸ�������*/
void GameOverDisplay();
//void Initialize();
void Delay_50ms();
void SwitchHandler(void * CallBackRef);
XGpio btns;
XIntc intCtrl;
int key;

int main()
{
	//��ʼ����ͼ����
    int Status;
    TftConfigPtr=XTft_LookupConfig(TFT_DEVICE_ID);
    Status=XTft_CfgInitialize(&TftInstance,TftConfigPtr,TftConfigPtr->BaseAddress);
    if(Status!=XST_SUCCESS) return XST_FAILURE;
    XTft_SetFrameBaseAddr(&TftInstance, TFT_FRAME_ADDR0);
    XTft_ClearScreen(&TftInstance);

    //��ʼ�������ն˿���
    Initialize();

    Xil_ICacheEnable();
    Xil_DCacheEnable();

	// Initialize SPI
	SPI_Init(SPI_BASEADDR, 0, 0, 0);

	// Software Reset
	ADXL362_WriteReg(ADXL362_SOFT_RESET, ADXL362_RESET_CMD);
	delay_ms(10);
	ADXL362_WriteReg(ADXL362_SOFT_RESET, 0x00);

	// Enable Measurement
	ADXL362_WriteReg(ADXL362_POWER_CTL, (2 << ADXL362_MEASURE));

    //̰������Ϸ����
    DrawK();

    PlaySnake();

    Xil_DCacheDisable();
    Xil_ICacheDisable();
    return XST_SUCCESS;
}
void DrawRectangle(int x,int y,int color){
    int i,j;
    for(i=x;i<x+xSIZE;i++)
    {
        for(j=y;j<y+ySIZE;j++)
        {
             XTft_SetPixel(&TftInstance,i,j,color);
        }
    }
}
/*��ʼ���棬���Ͻ�����Ϊ��50��40�������½�����Ϊ��610��460����Χǽ*/
void DrawK(void) {
    int i;
    int row1=xSTART;
    int row2=xEND;

    for(i=ySTART;i<=yEND;i++){
        DrawRectangle(row1*xSIZE,i*ySIZE,FGCOLOR_grn);
        DrawRectangle(row2*xSIZE,i*ySIZE,FGCOLOR_grn);
    }
    int col1=ySTART;
    int col2=yEND;
    for(i=xSTART;i<=xEND;i++){
       DrawRectangle(i*xSIZE,col1*ySIZE,FGCOLOR_grn);
       DrawRectangle(i*xSIZE,col2*ySIZE,FGCOLOR_grn);
    }
}

/*����Ϸ�������*/
void PlaySnake(void)
{
	int i;
	srand(ADXL362_ReadX());
    food.yes=1;/*1��ʾ��Ҫ������ʳ��,0��ʾ�Ѿ�����ʳ��*/
    snake.life=0;/*����*/
    snake.direction=1;/*��������*/
    snake.x[0]=(xSTART+xEND)/2;snake.y[0]=(ySTART+yEND)/2;/*��ͷ*/
    snake.x[1]=snake.x[0]+1;snake.y[1]=snake.y[0]+1;
    snake.node=2;/*����*/
    gamespeed = 50000;
    while(1)/*�����ظ�����Ϸ,ѹESC������*/
    {
    	while(key==0)/*��û�а����������,���Լ��ƶ�����*/
         {
             if(food.yes==1)/*��Ҫ������ʳ��*/
             {
            	 	cal_gamescore();
                    food.x=rand()%x_length+ 1;
                    food.y=rand()%y_length + 1;
                    food.yes=0;/*��������ʳ����*/
             }
             if(food.yes==0)/*��������ʳ���˾�Ҫ��ʾ*/
             {
                  DrawRectangle(food.x*xSIZE,food.y*ySIZE,0x3FF0494);
             }
             for(i=snake.node-1;i>0;i--)/*�ߵ�ÿ��������ǰ�ƶ�,Ҳ����̰���ߵĹؼ��㷨*/
             {
                snake.x[i]=snake.x[i-1];
                snake.y[i]=snake.y[i-1];
             }
              /*1,2,3,4��ʾ��,��,��,���ĸ�����,ͨ������ж����ƶ���ͷ*/
            switch(snake.direction)
            {
                case 1: snake.x[0]+=1;break;
                case 2: snake.x[0]-=1;break;
                case 3: snake.y[0]-=1;break;
                case 4: snake.y[0]+=1;break;
            }
            for(i=3;i<snake.node;i++)
            /*���ߵĵ��Ľڿ�ʼ�ж��Ƿ�ײ���Լ��ˣ���Ϊ��ͷΪ���ڣ������ڲ����ܹչ���*/
            {
                if(snake.x[i]==snake.x[0]&&snake.y[i]==snake.y[0])
                {
                    GameOver();/*��ʾʧ��*/
                    snake.life=1;
                    break;
                }
            }
            if(snake.x[0]<xSTART+1||snake.x[0]>xEND - 1||snake.y[0]<ySTART + 1||snake.y[0]>yEND - 1)/*���Ƿ�ײ��ǽ��*/
            {
                GameOver();/*������Ϸ����*/
                snake.life=1; /*����*/
                return;
            }
            if(snake.life==1)/*���������ж��Ժ�,���������������ѭ�������¿�ʼ*/
                break;

            xil_printf("%d,%d\r\n%d,%d\r\n",snake.x[0],food.x,snake.y[0],food.y);
            if(snake.x[0]==food.x&&snake.y[0]==food.y)/*�Ե�ʳ���Ժ�*/
            {
            	score = score + 1;
                DrawRectangle(food.x*xSIZE,food.y*ySIZE,0x0);
                xil_printf("eat\r\n");
                /*�ߵ����峤һ��*/
               snake.x[snake.node]=snake.x[snake.node-1];snake.y[snake.node]=snake.y[snake.node-1];

                snake.node++;
                //snake.x[0]=food.x;snake.y[0]=food.y;
                food.yes=1;/*��������Ҫ�����µ�ʳ��*/
            }

            for(i=0;i<snake.node;i++)//������
                 DrawRectangle(snake.x[i]*xSIZE,snake.y[i]*ySIZE,0xffffff);
            Delay(gamespeed);
            //ȥ���ߵ����һ��
            DrawRectangle(snake.x[snake.node-1]*xSIZE,snake.y[snake.node-1]*ySIZE,0x0);

            //��п���
            if(judge_Axis('y')==0)
			{
				if(snake.direction==1)
				{
					snake.direction=3;
				}
				else if(snake.direction==2)
				{
					snake.direction=4;
				}
				else if(snake.direction==3)
				{
					snake.direction=2;
				}
				else if(snake.direction==4)
				{
					snake.direction=1;
				}
				else
				{
					snake.direction=snake.direction;
				}
			}
			else if(judge_Axis('y')==2)
			{
				if(snake.direction==1)
				{
					snake.direction=4;
				}
				else if(snake.direction==2)
				{
					snake.direction=3;
				}
				else if(snake.direction==3)
				{
					snake.direction=1;
				}
				else if(snake.direction==4)
				{
					snake.direction=2;
				}
				else
				{
					snake.direction=snake.direction;
				}
			}
			else
			{
				snake.direction=snake.direction;
			}

            //�ٶȿ���
            if(judge_Axis('x')==2)
			{
				if(gamespeed!=0)
				{
					gamespeed = gamespeed - 5000;
				}
				else
				{
					gamespeed = 0;
				}
			}
			else if(judge_Axis('x')==0)
			{
				if(gamespeed!=100000)
				{
					gamespeed = gamespeed + 5000;
				}
				else
				{
					gamespeed = 100000;
				}
			}
			else
			{
				gamespeed = gamespeed;
			}
        }  /*endwhile����kbhit��*/

		if(snake.life==1)/*�������������ѭ��*/
			break;
		//��������
		//if(key==1)/*��ESC���˳�*/
			//break;
		else if(key==1&&snake.direction!=4) //up
			snake.direction=3;
		else if(key==2&&snake.direction!=1)//left
			snake.direction=2;
		else if(key==4&&snake.direction!=2)//right
			snake.direction=1;
		else if(key==8&&snake.direction!=3)//down
			snake.direction=4;
    }/*endwhile(1)*/
}
void Delay(int speed){
	int i;
	for(i=1;i<speed;i++);
}
void GameOver(){

	XTft_ClearScreen(&TftInstance);
	GameOverDisplay();
	cal_score();
}
void Initialize(){
	//��ʼ��Dipsʵ�������趨��Ϊ���뷽ʽ
	XGpio_Initialize(&btns,XPAR_BUTTON_DEVICE_ID);
	XGpio_SetDataDirection(&btns,1,0xff);

	//��ʼ��intCtrlʵ��
	XIntc_Initialize(&intCtrl,XPAR_AXI_INTC_0_DEVICE_ID);
	//GPIO�ж�ʹ��
	XGpio_InterruptEnable(&btns,1);
	XGpio_InterruptGlobalEnable(&btns);
	//���жϿ����������ж�Դʹ��
	XIntc_Enable(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR);
	//ע���жϷ�����
	XIntc_Connect(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR,(XInterruptHandler)SwitchHandler,(void *)0);

	Xil_Out32(XPAR_RS232_BASEADDR+0xC, (1 << 4));
	microblaze_register_handler((XInterruptHandler)uartIntHandler, (void *)&intCtrl);

	microblaze_enable_interrupts();
	//ע���жϿ�����������
//	microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler,(void *)&intCtrl);

	XIntc_Start(&intCtrl,XIN_REAL_MODE);//�����жϿ�����

}
void Delay_50ms(){
	int i;
	for(i=0;i<5000000;i++);
}
void SwitchHandler(void * CallBackRef){
	key=XGpio_DiscreteRead(&btns,1);//��ȡbuttons������״ֵ̬
	XGpio_InterruptClear(&btns,1);//����жϱ�־λ
}


void GameOverDisplay()
{
	char charSet[8]={'G','A','M','E','O','V','E','R'};
	int x,ratio=1;//ratioΪ�ַ����ű���
	int xPos=200,yPos=34;
	for(x=0;x<4;x++)
	{
		drawchar(xPos+30*x*ratio, yPos, charSet[x], ratio);
	}//GAME
	for(x=4;x<8;x++)
	{
		drawchar(xPos+30*x*ratio+30,yPos,charSet[x],ratio);
	}//OVER
}

void drawchar(int xPos,int yPos,char VarChar,int ratio)
{


	u8 modelG[5][5]={{0,1,1,1,0},
			{0,1,0,0,0},
			{0,1,0,0,0},
			{0,1,0,1,0},
			{0,1,1,1,0}};//G
	u8 modelA[5][5]={{0,0,1,0,0},
		{0,1,0,1,0},
		{0,1,1,1,0},
		{0,1,0,1,0},
		{0,1,0,1,0}};//A

	u8 modelM[5][5]={{1,0,0,0,1},
		{1,1,0,1,1},
		{1,0,1,0,1},
		{1,0,0,0,1},
		{1,0,0,0,1}};//M

	u8 modelO[5][5]={{0,0,1,1,0},
		{0,1,0,0,1},
		{0,1,0,0,1},
		{0,1,0,0,1},
		{0,0,1,1,0}};//O

	u8 modelV[5][5]={{0,1,0,1,0},
		{0,1,0,1,0},
		{0,1,0,1,0},
		{0,1,0,1,0},
		{0,0,1,0,0}};//V


	u8 modelR[5][5]={{0,1,1,1,0},
		{0,1,0,1,0},
		{0,1,1,0,0},
		{0,1,0,1,0},
		{0,1,0,1,0}};//R

	u8 modelE[5][5]={{0,1,1,1,0},
		{0,1,0,0,0},
		{0,1,1,1,0},
		{0,1,0,0,0},
		{0,1,1,1,0}};//E

	u8 model0[5][5] ={{0,1,1,1,0},
			{0,1,0,1,0},
			{0,1,0,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0}};

	u8 model1[5][5] = {{0,0,1,0,0},
			{0,1,1,0,0},
			{0,0,1,0,0},
			{0,0,1,0,0},
			{0,1,1,1,0}};


	u8 model2[5][5] = {{0,1,1,1,0},
			{0,0,0,1,0},
			{0,1,1,1,0},
			{0,1,0,0,0},
			{0,1,1,1,0}};


	u8 model3[5][5] = {{0,1,1,1,0},
			{0,0,0,1,0},
			{0,1,1,1,0},
			{0,0,0,1,0},
			{0,1,1,1,0}};


	u8 model4[5][5] = {{0,1,0,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0},
			{0,0,0,1,0},
			{0,0,0,1,0}};

	u8 model5[5][5] = {{0,1,1,1,0},
			{0,1,0,0,0},
			{0,1,1,1,0},
			{0,0,0,1,0},
			{0,1,1,1,0}};

	u8 model6[5][5] = {{0,1,1,1,0},
			{0,1,0,0,0},
			{0,1,1,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0}};

	u8 model8[5][5] = {{0,1,1,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0}};
	u8 model7[5][5] = {{0,1,1,1,0},
			{0,0,0,1,0},
			{0,0,0,1,0},
			{0,0,0,1,0},
			{0,0,0,1,0}};
	u8 model9[5][5] = {{0,1,1,1,0},
			{0,1,0,1,0},
			{0,1,1,1,0},
			{0,0,0,1,0},
			{0,1,1,1,0}};

	int x,y;
	switch(VarChar){
	case 'G':
		for(y=yPos;y<yPos+5*ratio;y++)
			{
				for(x=xPos;x<xPos+20*ratio;x++)
				{
					if(modelG[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
				}
			}
		break;

	case 'A':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelA[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;


	case 'M':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelM[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case 'E':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelE[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case 'O':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelO[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case 'V':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelV[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;

	case 'R':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(modelR[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '0':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model0[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '1':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model1[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '2':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model2[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '3':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model3[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '4':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model4[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '5':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model5[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '6':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model6[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '8':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model8[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;

	case '7':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model7[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
			break;
	case '9':
			for(y=yPos;y<yPos+5*ratio;y++)
				{
					for(x=xPos;x<xPos+20*ratio;x++)
					{
						if(model9[(y-yPos)/ratio][(x-xPos)/(4*ratio)]) XTft_SetPixel(&TftInstance,x,y,0xFFFFFF);
					}
				}
				break;
	}
}

void cal_score(){
	int bit1 = 0, bit0 = 0;
	bit1 = score/10;
	bit0 = score - 10*bit1;

	drawchar(300,44,bit1+0x30,1);
	drawchar(320,44,bit0+0x30,1);
}

void cal_gamescore(){
	int bit1 = 0, bit0 = 0;
	bit1 = score/10;
	bit0 = score - 10*bit1;
	//int x,y;
	XTft_FillScreen(&TftInstance,604,44,639,49,0x0);

	drawchar(604,44,bit1+0x30,1);
	drawchar(620,44,bit0+0x30,1);
}


