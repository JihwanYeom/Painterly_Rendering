#pragma warning(disable:4996)
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <opencv2/opencv.hpp>

//Stroke 구조체 정의
typedef struct {
	int r,x,y; // 브러쉬 사이즈, 원의 x,y좌표
	int point[20][2]; // Spline의 각 지점
	int length; // Stroke 길이
	CvScalar color; // Stroke의 색상
}Stroke;

char* inputFileName(); // 파일 이름을 입력받는 함수
IplImage* paint(IplImage* src, int R[]); // 브러쉬 사이즈 마다 printLayer 함수를 이용해 그림을 그려 canvas를 반환하는 함수
void paintLayer(IplImage* canvas, IplImage* ref, int R); // makeStroke 함수를 이용해 생성한 Stroke를 이용해 한 레이어를 그리는 함수
Stroke makeStroke(int y0, int x0, int R, IplImage* ref); // 원형의 Stroke를 만드는 함수
Stroke makeSplineStroke(int y0, int x0, int R, IplImage* ref, IplImage* canvas); // Spline Stroke를 만드는 함수

int MODE = 0; // 렌더링 스타일 결정 변수

int main()
{
	srand(time(NULL)); //seed 초기화 

	int R[5] = { 32, 16, 8, 4 ,2 }; // Brush Size 정의

	// 파일 경로 입력받아 이미지 불러오기
	char *path = inputFileName(); 
	IplImage* img = cvLoadImage(path); 

	// Drowing Mode 입력받기
	printf("Select Drawing Mode (0=circle, 1=stroke): ");
	scanf("%d", &MODE);

	//원본 이미지 출력 후 paint함수를 이용해 painterly rendering 진행
	cvShowImage("img", img);
	IplImage* canvas = paint(img, R);
	cvShowImage("canvas", canvas);
	cvSaveImage("C:\\multimedia\\result3.jpg", canvas);
	cvWaitKey();
}

IplImage* paint(IplImage* src, int R[]) 
{
	// 그림을 그릴 canvas 와 필터링한 이미지를 저장할 ref 선언
	CvSize size = cvGetSize(src);
	IplImage* canvas = cvCreateImage(size, 8, 3);
	IplImage* ref = cvCreateImage(size, 8, 3);
	cvSet(canvas, cvScalar(255, 255, 255)); // canvas를 흰색으로 초기화

	// fs = 3으로 선언(브러쉬 사이즈의 몇배 크기의 커널을 이용해 필터릴 할지 결정하는 상수)
	float fs = 3;
	
	// 각 브러쉬 사이즈마다 이미지를 필터링하여 printLayer함수를 이용해 그림 그리기
	for (int i = 0; i < 5; i++)
	{
		cvSmooth(src, ref, CV_GAUSSIAN, fs*R[i]+1); // src를 가우시안 필터로 블러처리하여 ref에 저장. 브러쉬 사이즈는 전부 짝수인데 커널 사이즈가 홀수여야 하므로 +1을 통해 처리함
		paintLayer(canvas, ref, R[i]); //printLayer함수에 브러쉬사이즈 전달
	}
	// 그림이 그려진 canvas 반환
	return canvas;
}

void paintLayer(IplImage* canvas, IplImage* ref, int R)
{
	// ref의 너비와 높이를 w와 h에 저장
	CvSize size = cvGetSize(ref);
	int w = size.width;
	int h = size.height;

	// canvas 와 ref의 각 픽셀 사이의 오차를 저장할 2차원 배열 D를 ref의 픽셀 수만큼 할당
	int** D = (int**)malloc(sizeof(int*)*h);
	for (int i = 0; i < h; i++)
		D[i] = (int*)malloc(sizeof(int) * w);

	// ref와 canvas의 각 픽셀의 오차를 계산하여 D에 저장
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			D[y][x] = 0;
			for (int k = 0; k < 3; k++) {
				D[y][x] += abs(cvGet2D(canvas, y, x).val[k] - cvGet2D(ref, y, x).val[k]);
			}
		}
	}

	// 그리드 사이즈 정하기
	float fg = 1; // fg = 1로 선언(브러쉬 사이즈의 몇배를 그리드 하나의 사이즈로 정할지 결정하는 상수)
	int grid = fg*R; // 그리드 사이즈 결정
	int grid_x = w / grid; // x축 방향 그리드의 개수 저장
	int grid_y = h / grid; // y축 방향 그리드의 개수 저장

	// Stroke 배열을 그리드의 개수만큼 할당
	Stroke* S = (Stroke*)malloc(sizeof(Stroke) * (grid_x*grid_y));
	int stroke_cnt = 0; // Stroke의 갯수를 저장할 변수 stroke_cnt
	
	int T = 25; // Stroke를 생성되는 임계값 T는 25로 선언

	// 각 그리드를 순회하며 그리드의 오차 계산
	for (int y = grid/2 ; y < grid*grid_y; y += grid)
	{
		for (int x = grid / 2; x < grid*grid_x; x += grid)
		{
			int max_error = 0; // 그리드 내에서의 최대 오차
			int max_x = 0; // 그리드 내 최대 오차를 가진 부분의 x좌표
			int max_y = 0; // 그리드 내 최대 오차를 가진 부분의 x좌표
			int grid_error = 0; // 그리드 내의 오차의 평균

			// 그리드를 순회하며 오차의 최댓값과 오차의 총합 연산
			for (int u = y - (grid / 2); u < y + (grid / 2)-1; u++) 
			{
				for (int v = x - (grid / 2); v < x + (grid / 2)-1; v++)
				{
					// 오차값이 max_error보다 클 경우 max_error와 max_x,max_y 갱신
   					if (D[u][v] > max_error) {
						max_error = D[u][v];
						max_x = v;
						max_y = u;
					}
					// 오차값을 grid_error에 더함
					grid_error += D[u][v];
				}
			}
			grid_error /= (grid * grid); // grid_error를 grid의 제곱으로 나누어 평균을 구함

			// grid_error가 임계값보다 클 경우
			if (grid_error > T) {
				if (MODE == 0) // mode가 1일 땐 makeStroke로 max_x, max_y 위치의 원형 Stroke를 생성하여 S에 추가
					S[stroke_cnt] = makeStroke(max_y, max_x, R, ref);
				else if (MODE == 1) // mode가 1일 땐 makeSplineStroke로 max_x, max_y 위치의 Spline Stroke를 생성하여 S에 추가
					S[stroke_cnt] = makeSplineStroke(max_y, max_x, R, ref, canvas);
				stroke_cnt++; // stroke_cnt 1 증가
			}
		}
	}

	// 랜덤 순서 만들기
	int* order = (int*)malloc(sizeof(int) * grid_x * grid_y); // 그리드 개수만큼 int형 배열 order을 동적으로 할당
	//order를 순회하며 stroke_cnt 보다 작은 수를 무작위로 저장
	for (int i = 0; i < stroke_cnt; i++)
	{
		order[i] = rand() % (stroke_cnt);
		//이미 저장되어있는 수일 경우 무시하고 다시 저장하게 하여 중복을 피함
		for (int j = 0; j < i; j++)
		{
			if (order[i] == order[j])
			{
				i--;
				break;
			}
		}
	}

	//모드에 따라 canvas에 Stroke를 이용해 무작위 순서로 원 또는 선을 그림
	if (MODE == 0) 
	{
		for (int i = 0; i < stroke_cnt; i++)
		{
			Stroke K = S[order[i]];
			cvCircle(canvas, cvPoint(K.x, K.y), R, K.color, -1); // canvas에 Stroke의 x, y좌표에 꽉찬 원을 그림
		}
	}
	else if (MODE == 1)
	{
		for (int i = 0; i < stroke_cnt; i++)
		{
			Stroke K = S[order[i]];
			//Stroke의 길이가 최소길이보다 길 경우 Stroke의 point배열에 저장된 지점들을 따라 선을 그어 spline을 형성
			if (K.length >= 4)
			{
				for (int j = 0; j < K.length - 1; j++)
					cvLine(canvas, cvPoint(K.point[j][0], K.point[j][1]), cvPoint(K.point[j + 1][0], K.point[j + 1][1]), K.color, R);
			}
		}
	}

	// 동적할당 해제
	for (int i = 0; i < h; i++)
		free(D[i]);
	free(D);
	free(S);
	free(order);

	// canvas를 출력하고 1초 후 다시 함수 종료
	cvShowImage("canvas", canvas);
	cvWaitKey(1000);
}

Stroke makeStroke(int y, int x, int R, IplImage* ref)
{
	Stroke K; // 반환할 Stroke K 선언
	K.r = R; // r에 브러쉬 사이즈 저장
	K.color = cvGet2D(ref,y,x); // ref에서 전달받은 (max_x, max_y)픽셀의 컬러를 가져와 저장
	K.y = y; // Stroke를 그릴 위치인 max_y 저장
	K.x = x; // Stroke를 그릴 위치인 max_x 저장

	return K; // K 반환
}

Stroke makeSplineStroke(int y0, int x0, int R, IplImage* ref, IplImage* canvas)
{
	int maxStrokeLength = 16; // Spline Stroke의 최대 길이
	int minStrokeLength = 4; // Spline Stroke의 최소 길이
	float fc = 0.5; // fc = 0.5로 선언 (곡률 조절 상수)
	Stroke K; // 반환할 Stroke K 선언
	K.r = R; // r에 브러쉬 사이즈 저장
	K.color = cvGet2D(ref, y0, x0); // ref에서 전달받은 (max_x, max_y)픽셀의 컬러를 가져와 저장
	K.point[0][0] = x0; // Stroke를 그리기 시작할 위치인 max_x를 첫번째 포인트로 저장
	K.point[0][1] = y0; // Stroke를 그리기 시작할 위치인 max_y를 첫번째 포인트로 저장
	int y = y0; // gx, gy를 계산할 현재 점 y를 max_y로 초기화
	int x = x0; // gx, gy를 계산할 현재 점 x를 max_x로 초기화
	float lastDy = 0; // 이전 포인트에서 구한 dy를 저장할 변수 lastDy를 0으로 초기화
	float lastDx = 0; // 이전 포인트에서 구한 dx를 저장할 변수 lastDx를 0으로 초기화

	int i;
	for (i = 1; i < maxStrokeLength; i++)
	{
		// x, y가 1보다 작거나 각각 width-2, heigth-2보다 클 경우 함수 종료
		if (x < 1 || y < 1 || x > ref->width-2 || y > ref->height-2) {
			K.length = i;
			return K;
		}

		int r_cError = 0; // ref 와 canvas 사이의 오차
		int r_sError = 0; // ref 와 Stroke 사이의 오차

		// ref 와 canvas 사이의 오차 계산
		for (int k = 0; k < 3; k++)
			r_cError += abs(cvGet2D(ref, y, x).val[k] - cvGet2D(canvas, y, x).val[k]);
		// ref 와 canvas 사이의 오차 계산
		for (int k = 0; k < 3; k++)
			r_sError += abs(cvGet2D(ref, y, x).val[k] - K.color.val[k]);

		// i가 최소 스트로크 길이보다 길고 ref와 canvas의 오차보다 ref와 stroke의 오차가 클 경우 함수 종료
		if (i > minStrokeLength && r_cError < r_sError) {
			K.length = i;
			return K;
		}

		float gy = 0; // y방향 색상 변화량 gy
		float gx = 0; // x방향 색상 변화량 gx

		// x, y를 기준으로 상,하,좌,우의 색 변화량을 계산하여 y방향 색상 변화량, x방향 색상 변화량을 구해 벡터(gx,gy) 구하기
		for (int k = 0; k < 3; k++)
			gx += cvGet2D(ref, y, x+1).val[k] - cvGet2D(ref, y, x-1).val[k];
		for (int k = 0; k < 3; k++)
			gy += cvGet2D(ref, y+1, x).val[k] - cvGet2D(ref, y-1, x).val[k];
		
		// gx와 gy가 모두 0일 경우 함수 종료
		if (sqrt(gx * gx + gy * gy) == 0) {
			K.length = i;
			return K;
		}

		// 색상이 변하는 방향의 벡터 (gx,gy)와 수직인 벡터(dx,dy)를 계산
		float dx = -gy;
		float dy = gx;

		// dx,dy의 방향이 lastDx, lastDy의 방향과 반대일 경우 방향을 반대로 설정
		if (lastDx * dx + lastDy * dy < 0) {
			dx = -dx;
			dy = -dy;
		}

		// 상수 fc를 통해 dx,dy 와 lastDx,lastDy의 비율을 조절해 더하여 dx dy에 저장하여 곡률 조정
		dx = fc * dx + (1 - fc) * lastDx;
		dy = fc * dy + (1 - fc) * lastDy;

		// (dx,dy)를 단위벡터로 만드는 연산
		dx = dx / sqrt(dx * dx + dy * dy);
		dy = dy / sqrt(dx * dx + dy * dy);

		//브러쉬 사이즈 만큼 (dx,dy) 방향으로 포인트 이동
		x = x + R * dx;
		y = y + R * dy;

		//lastDx, lastDy에 dx dy 저장
		lastDx = dx;
		lastDy = dy;

		// (x,y) 를 K.point에 추가하여 spline의 한 점 추가
		K.point[i][0] = x;
		K.point[i][1] = y;
	}
	K.length = i; // K.length에 i 저장
	return K; // 완성된 Spline Stroke K 반환
}

char* inputFileName()
{
	char* str = (char*)malloc(sizeof(char)); // 크기가 1인 char형 배열 동적할당
	printf("================================\nMultimedia Programming Homework4\n<Painterly Rendering>\n21011778 Yeom Jihwan\n================================\n");
	printf("Input File Path: ");

	// 문자를 순차적으로 입력받으며 문자가 늘어날 때 마다 배열을 다시 할당하여 저장
	for (int i = 1;; i++)
	{
		str = (char*)realloc(str, sizeof(char) * i);
		scanf("%c", &str[i - 1]);
		if (str[i - 1] == '\n')
		{
			str[i - 1] = '\0';
			break;
		}
	}

	// 문자열 반환
	return str;
}