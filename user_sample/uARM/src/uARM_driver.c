/*
	文件：uARM_driver.c
	说明：uARM驱动源文件
	作者：SchumyHao
	版本：V02
	日期：2013.03.18
*/
#define DEBUG

/* 头文件 */
#include "uARM_driver.h"


/* 函数 */
/* 发送控制指令函数 */
int SendData(FILE* const pFp, int const BuffDeep, const char* pBuff){
	int i,j;
	/*#ifdef DEBUG
	for(j=0; j<BuffDeep; j++){
		printf("%4d	",j+1);
		for (i=0; i<BUFFER_SIZE; i++){
			printf("0x%2x ",pBuff[j*BUFFER_SIZE+i]);
		}
		printf("\n");
	}
	#endif*/
	for(j=0; j<BuffDeep; j++){
		for (i=0; i<BUFFER_SIZE; i++){
			if(fputc(pBuff[j*BUFFER_SIZE+i], pFp) == -1)
				break;
		}
		if (i < BUFFER_SIZE){		//没有全部发送
			perror("Send data incomplete.\n");
			return -1;
		}
		usleep(FRAME_DELAY_TIME);
	}
	return 0;
}
/* 坐标系结构体参数初始化 */
int InitCoordinateSystem(t_coordinate* pCooSys){
	pCooSys->X = DEFAULT_X_LOCATION;
	pCooSys->Y = DEFAULT_Y_LOCATION;
	pCooSys->Angle = DEFAULT_A_DEGREE;
	pCooSys->Radius = DEFAULT_R_LENGTH;
	pCooSys->DestAngle = DEFAULT_DEST_A;
	pCooSys->DestRadius = DEFAULT_DEST_R;
	pCooSys->CurrAngle = DEFAULT_A_DEGREE;
	pCooSys->CurrRadius = DEFAULT_R_LENGTH;
	return 0;
}
/* 坐标系参数变换 */
int ShiftCoordinate(t_coordinate* pCooSys){
	assert(IS_X_LOCATION(pCooSys->X));
	assert(IS_Y_LOCATION(pCooSys->Y));
	int DistX = pCooSys->X - UARM_X_LOCATION;
	int DistY = pCooSys->Y - UARM_Y_LOCATION;
	pCooSys->Radius = (int)sqrt((double)((DistX*DistX)+(DistY*DistY)));
	pCooSys->Angle = (int)((DistY)?(RAD2ANG(atan2((double)DistX,(double)DistY))): \
			((DistX>0)?MAX_A_DEGREE:((DistX<0)?MIN_A_DEGREE:DEFAULT_A_DEGREE)));
	return 0;
}
/* 动作生成 */
int GenerateMotion(t_coordinate* pCooSys, char* pBuff){
	assert(IS_A_DEGREE(pCooSys->Angle));
	assert(IS_R_LENGTH(pCooSys->Radius));
	assert(IS_DESTINATION_A(pCooSys->DestAngle));
	assert(IS_DESTINATION_R(pCooSys->DestRadius));
	#ifdef GO_WITH_LINE
	int TempA,TempR,TempH;
	int MaxLoop;
	int StepA,StepR,StepH;
	int i;
	int BuffDeep = 0;
	/* 去捡钱 */
	//计算Angle、Radius、Hight各自差
	TempA = pCooSys->Angle - pCooSys->CurrAngle;
	TempR = pCooSys->Radius - pCooSys->CurrRadius;
	TempH = MIN_H_POSITION - MAX_H_POSITION;	//从上至下
	//计算最大的循环周期
	MaxLoop = MAX2(abs(TempA),(MAX2(abs(TempR),abs(TempH))));
	//计算步长
	StepA = abs((TempA)?(MaxLoop/TempA):MaxLoop);
	StepR = abs((TempR)?(MaxLoop/TempR):MaxLoop);
	StepH = abs((TempH)?(MaxLoop/TempH):MaxLoop);
	#ifdef DEBUG
	printf("\nBefor go to pick the coin.\n");
	printf("Current Angle is %d.\n",pCooSys->CurrAngle);
	printf("Current Radius is %d.\n",pCooSys->CurrRadius);
	printf("Current Hight is %d.\n",MAX_H_POSITION);
	printf("Destination Angle is %d.\n",pCooSys->Angle);
	printf("Destination Radius is %d.\n",pCooSys->Radius);
	printf("Destination Hight is %d.\n",MIN_H_POSITION);
	printf("TempA is DestA-CurrA = %d.\n",TempA);
	printf("TempR is DestR-CurrR = %d.\n",TempR);
	printf("TempH is DestH-CurrH = %d.\n",TempH);
	printf("MaxLoop is %d.\n",MaxLoop);
	printf("StepA is %d.\n",StepA);
	printf("StepR is %d.\n",StepR);
	printf("StepH is %d.\n",StepH);
	#endif
	//循环赋值
	for(i=MaxLoop; i>0; i--){
		//赋值
		pCooSys->CurrAngle += ((i%StepA) || (!TempA))? \
				0:(SINGNAL(TempA));
		pCooSys->CurrRadius += ((i%StepR) || (!TempR))? \
				0:(SINGNAL(TempR));
		TempA -= ((i%StepA)||(!TempA))?0:(SINGNAL(TempA));
		TempR -= ((i%StepR)||(!TempR))?0:(SINGNAL(TempR));
		TempH -= ((i%StepH)||(!TempH))?0:(SINGNAL(TempH));

		*pBuff++ = FRAME_HEADER_H;
		*pBuff++ = FRAME_HEADER_L;
		*pBuff++ = HI_BYTE(pCooSys->CurrAngle);
		*pBuff++ = LO_BYTE(pCooSys->CurrAngle);
		*pBuff++ = HI_BYTE(pCooSys->CurrRadius);
		*pBuff++ = LO_BYTE(pCooSys->CurrRadius);
		*pBuff++ = ((i%StepH)||(!TempH))?MOTION_NONE:MOTION_H_DOWN;
		//缓冲深度加一
		BuffDeep++;
	}
	#ifdef DEBUG
	printf("BuffDeep in go to pick is %d.\n",BuffDeep);
	#endif
	/* 捡钱 */
	//吸取硬币
	#ifdef DEBUG
	printf("\nBefor pick coin.\n");
	printf("Current Angle is %d.\n",pCooSys->CurrAngle);
	printf("Current Radius is %d.\n",pCooSys->CurrRadius);
	printf("Current Hight is %d.\n",MIN_H_POSITION);
	#endif
	for(i=PICK_RETRY_TIMES; i>0; i--){
		*pBuff++ = FRAME_HEADER_H;
		*pBuff++ = FRAME_HEADER_L;
		*pBuff++ = HI_BYTE(pCooSys->CurrAngle);
		*pBuff++ = LO_BYTE(pCooSys->CurrAngle);
		*pBuff++ = HI_BYTE(pCooSys->CurrRadius);
		*pBuff++ = LO_BYTE(pCooSys->CurrRadius);
		*pBuff++ = MOTION_PICK;
		BuffDeep++;
	}
	#ifdef DEBUG
	printf("BuffDeep in pick is %d.\n",BuffDeep);
	#endif
	/* 到马上 */
	//计算Angle、Radius、Hight各自差。此时Curr位置和硬币位置相同
	TempA = pCooSys->DestAngle - pCooSys->CurrAngle;
	TempR = pCooSys->DestRadius - pCooSys->CurrRadius;
	TempH = MAX_H_POSITION - MIN_H_POSITION;	//自下往上

	MaxLoop = MAX2(abs(TempA),(MAX2(abs(TempR),abs(TempH))));

	StepA = abs((TempA)?(MaxLoop/TempA):MaxLoop);
	StepR = abs((TempR)?(MaxLoop/TempR):MaxLoop);
	StepH = abs((TempH)?(MaxLoop/TempH):MaxLoop);
	#ifdef DEBUG
	printf("\nBefor go to horse.\n");
	printf("Current Angle is %d.\n",pCooSys->CurrAngle);
	printf("Current Radius is %d.\n",pCooSys->CurrRadius);
	printf("Current Hight is %d.\n",MIN_H_POSITION);
	printf("Destination Angle is %d.\n",pCooSys->DestAngle);
	printf("Destination Radius is %d.\n",pCooSys->DestRadius);
	printf("Destination Hight is %d.\n",MAX_H_POSITION);
	printf("TempA is DestA-CurrA = %d.\n",TempA);
	printf("TempR is DestR-CurrR = %d.\n",TempR);
	printf("TempH is DestH-CurrH = %d.\n",TempH);
	printf("\nMaxLoop is %d.\n",MaxLoop);
	printf("StepA is %d.\n",StepA);
	printf("StepR is %d.\n",StepR);
	printf("StepH is %d.\n",StepH);
	#endif
	for(i=MaxLoop; i>0; i--){
		pCooSys->CurrAngle += ((i%StepA) || (!TempA))? \
				0:(SINGNAL(TempA));
		pCooSys->CurrRadius += ((i%StepR) || (!TempR))? \
				0:(SINGNAL(TempR));
		TempA -= ((i%StepA)||(!TempA))?0:(SINGNAL(TempA));
		TempR -= ((i%StepR)||(!TempR))?0:(SINGNAL(TempR));
		TempH -= ((i%StepH)||(!TempH))?0:(SINGNAL(TempH));

		*pBuff++ = FRAME_HEADER_H;
		*pBuff++ = FRAME_HEADER_L;
		*pBuff++ = HI_BYTE(pCooSys->CurrAngle);
		*pBuff++ = LO_BYTE(pCooSys->CurrAngle);
		*pBuff++ = HI_BYTE(pCooSys->CurrRadius);
		*pBuff++ = LO_BYTE(pCooSys->CurrRadius);
		*pBuff++ = ((i%StepH)||(!TempH))?MOTION_NONE:MOTION_H_UP;
		BuffDeep++;
	}
	#ifdef DEBUG
	printf("BuffDeep in go to horse is %d.\n",BuffDeep);
	#endif
	/* 放下 */
	#ifdef DEBUG
	printf("\nBefor drop cion.\n");
	printf("Current Angle is %d.\n",pCooSys->CurrAngle);
	printf("Current Radius is %d.\n",pCooSys->CurrRadius);
	printf("Current Hight is %d.\n",MAX_H_POSITION);
	#endif
	*pBuff++ = FRAME_HEADER_H;
	*pBuff++ = FRAME_HEADER_L;
	*pBuff++ = HI_BYTE(pCooSys->CurrAngle);
	*pBuff++ = LO_BYTE(pCooSys->CurrAngle);
	*pBuff++ = HI_BYTE(pCooSys->CurrRadius);
	*pBuff++ = LO_BYTE(pCooSys->CurrRadius);
	*pBuff++ = MOTION_RELEASE;
	//缓冲深度加一
	BuffDeep++;
	#ifdef DEBUG
	printf("BuffDeep in drop is %d.\n",BuffDeep);
	#endif
	#endif
	#ifdef GO_WITH_PARABOLA
	#endif
	return BuffDeep;
}