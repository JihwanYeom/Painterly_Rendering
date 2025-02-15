#pragma warning(disable:4996)
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <opencv2/opencv.hpp>

//Stroke ����ü ����
typedef struct {
	int r,x,y; // �귯�� ������, ���� x,y��ǥ
	int point[20][2]; // Spline�� �� ����
	int length; // Stroke ����
	CvScalar color; // Stroke�� ����
}Stroke;

char* inputFileName(); // ���� �̸��� �Է¹޴� �Լ�
IplImage* paint(IplImage* src, int R[]); // �귯�� ������ ���� printLayer �Լ��� �̿��� �׸��� �׷� canvas�� ��ȯ�ϴ� �Լ�
void paintLayer(IplImage* canvas, IplImage* ref, int R); // makeStroke �Լ��� �̿��� ������ Stroke�� �̿��� �� ���̾ �׸��� �Լ�
Stroke makeStroke(int y0, int x0, int R, IplImage* ref); // ������ Stroke�� ����� �Լ�
Stroke makeSplineStroke(int y0, int x0, int R, IplImage* ref, IplImage* canvas); // Spline Stroke�� ����� �Լ�

int MODE = 0; // ������ ��Ÿ�� ���� ����

int main()
{
	srand(time(NULL)); //seed �ʱ�ȭ 

	int R[5] = { 32, 16, 8, 4 ,2 }; // Brush Size ����

	// ���� ��� �Է¹޾� �̹��� �ҷ�����
	char *path = inputFileName(); 
	IplImage* img = cvLoadImage(path); 

	// Drowing Mode �Է¹ޱ�
	printf("Select Drawing Mode (0=circle, 1=stroke): ");
	scanf("%d", &MODE);

	//���� �̹��� ��� �� paint�Լ��� �̿��� painterly rendering ����
	cvShowImage("img", img);
	IplImage* canvas = paint(img, R);
	cvShowImage("canvas", canvas);
	cvSaveImage("C:\\multimedia\\result3.jpg", canvas);
	cvWaitKey();
}

IplImage* paint(IplImage* src, int R[]) 
{
	// �׸��� �׸� canvas �� ���͸��� �̹����� ������ ref ����
	CvSize size = cvGetSize(src);
	IplImage* canvas = cvCreateImage(size, 8, 3);
	IplImage* ref = cvCreateImage(size, 8, 3);
	cvSet(canvas, cvScalar(255, 255, 255)); // canvas�� ������� �ʱ�ȭ

	// fs = 3���� ����(�귯�� �������� ��� ũ���� Ŀ���� �̿��� ���͸� ���� �����ϴ� ���)
	float fs = 3;
	
	// �� �귯�� ������� �̹����� ���͸��Ͽ� printLayer�Լ��� �̿��� �׸� �׸���
	for (int i = 0; i < 5; i++)
	{
		cvSmooth(src, ref, CV_GAUSSIAN, fs*R[i]+1); // src�� ����þ� ���ͷ� ��ó���Ͽ� ref�� ����. �귯�� ������� ���� ¦���ε� Ŀ�� ����� Ȧ������ �ϹǷ� +1�� ���� ó����
		paintLayer(canvas, ref, R[i]); //printLayer�Լ��� �귯�������� ����
	}
	// �׸��� �׷��� canvas ��ȯ
	return canvas;
}

void paintLayer(IplImage* canvas, IplImage* ref, int R)
{
	// ref�� �ʺ�� ���̸� w�� h�� ����
	CvSize size = cvGetSize(ref);
	int w = size.width;
	int h = size.height;

	// canvas �� ref�� �� �ȼ� ������ ������ ������ 2���� �迭 D�� ref�� �ȼ� ����ŭ �Ҵ�
	int** D = (int**)malloc(sizeof(int*)*h);
	for (int i = 0; i < h; i++)
		D[i] = (int*)malloc(sizeof(int) * w);

	// ref�� canvas�� �� �ȼ��� ������ ����Ͽ� D�� ����
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

	// �׸��� ������ ���ϱ�
	float fg = 1; // fg = 1�� ����(�귯�� �������� ��踦 �׸��� �ϳ��� ������� ������ �����ϴ� ���)
	int grid = fg*R; // �׸��� ������ ����
	int grid_x = w / grid; // x�� ���� �׸����� ���� ����
	int grid_y = h / grid; // y�� ���� �׸����� ���� ����

	// Stroke �迭�� �׸����� ������ŭ �Ҵ�
	Stroke* S = (Stroke*)malloc(sizeof(Stroke) * (grid_x*grid_y));
	int stroke_cnt = 0; // Stroke�� ������ ������ ���� stroke_cnt
	
	int T = 25; // Stroke�� �����Ǵ� �Ӱ谪 T�� 25�� ����

	// �� �׸��带 ��ȸ�ϸ� �׸����� ���� ���
	for (int y = grid/2 ; y < grid*grid_y; y += grid)
	{
		for (int x = grid / 2; x < grid*grid_x; x += grid)
		{
			int max_error = 0; // �׸��� �������� �ִ� ����
			int max_x = 0; // �׸��� �� �ִ� ������ ���� �κ��� x��ǥ
			int max_y = 0; // �׸��� �� �ִ� ������ ���� �κ��� x��ǥ
			int grid_error = 0; // �׸��� ���� ������ ���

			// �׸��带 ��ȸ�ϸ� ������ �ִ񰪰� ������ ���� ����
			for (int u = y - (grid / 2); u < y + (grid / 2)-1; u++) 
			{
				for (int v = x - (grid / 2); v < x + (grid / 2)-1; v++)
				{
					// �������� max_error���� Ŭ ��� max_error�� max_x,max_y ����
   					if (D[u][v] > max_error) {
						max_error = D[u][v];
						max_x = v;
						max_y = u;
					}
					// �������� grid_error�� ����
					grid_error += D[u][v];
				}
			}
			grid_error /= (grid * grid); // grid_error�� grid�� �������� ������ ����� ����

			// grid_error�� �Ӱ谪���� Ŭ ���
			if (grid_error > T) {
				if (MODE == 0) // mode�� 1�� �� makeStroke�� max_x, max_y ��ġ�� ���� Stroke�� �����Ͽ� S�� �߰�
					S[stroke_cnt] = makeStroke(max_y, max_x, R, ref);
				else if (MODE == 1) // mode�� 1�� �� makeSplineStroke�� max_x, max_y ��ġ�� Spline Stroke�� �����Ͽ� S�� �߰�
					S[stroke_cnt] = makeSplineStroke(max_y, max_x, R, ref, canvas);
				stroke_cnt++; // stroke_cnt 1 ����
			}
		}
	}

	// ���� ���� �����
	int* order = (int*)malloc(sizeof(int) * grid_x * grid_y); // �׸��� ������ŭ int�� �迭 order�� �������� �Ҵ�
	//order�� ��ȸ�ϸ� stroke_cnt ���� ���� ���� �������� ����
	for (int i = 0; i < stroke_cnt; i++)
	{
		order[i] = rand() % (stroke_cnt);
		//�̹� ����Ǿ��ִ� ���� ��� �����ϰ� �ٽ� �����ϰ� �Ͽ� �ߺ��� ����
		for (int j = 0; j < i; j++)
		{
			if (order[i] == order[j])
			{
				i--;
				break;
			}
		}
	}

	//��忡 ���� canvas�� Stroke�� �̿��� ������ ������ �� �Ǵ� ���� �׸�
	if (MODE == 0) 
	{
		for (int i = 0; i < stroke_cnt; i++)
		{
			Stroke K = S[order[i]];
			cvCircle(canvas, cvPoint(K.x, K.y), R, K.color, -1); // canvas�� Stroke�� x, y��ǥ�� ���� ���� �׸�
		}
	}
	else if (MODE == 1)
	{
		for (int i = 0; i < stroke_cnt; i++)
		{
			Stroke K = S[order[i]];
			//Stroke�� ���̰� �ּұ��̺��� �� ��� Stroke�� point�迭�� ����� �������� ���� ���� �׾� spline�� ����
			if (K.length >= 4)
			{
				for (int j = 0; j < K.length - 1; j++)
					cvLine(canvas, cvPoint(K.point[j][0], K.point[j][1]), cvPoint(K.point[j + 1][0], K.point[j + 1][1]), K.color, R);
			}
		}
	}

	// �����Ҵ� ����
	for (int i = 0; i < h; i++)
		free(D[i]);
	free(D);
	free(S);
	free(order);

	// canvas�� ����ϰ� 1�� �� �ٽ� �Լ� ����
	cvShowImage("canvas", canvas);
	cvWaitKey(1000);
}

Stroke makeStroke(int y, int x, int R, IplImage* ref)
{
	Stroke K; // ��ȯ�� Stroke K ����
	K.r = R; // r�� �귯�� ������ ����
	K.color = cvGet2D(ref,y,x); // ref���� ���޹��� (max_x, max_y)�ȼ��� �÷��� ������ ����
	K.y = y; // Stroke�� �׸� ��ġ�� max_y ����
	K.x = x; // Stroke�� �׸� ��ġ�� max_x ����

	return K; // K ��ȯ
}

Stroke makeSplineStroke(int y0, int x0, int R, IplImage* ref, IplImage* canvas)
{
	int maxStrokeLength = 16; // Spline Stroke�� �ִ� ����
	int minStrokeLength = 4; // Spline Stroke�� �ּ� ����
	float fc = 0.5; // fc = 0.5�� ���� (��� ���� ���)
	Stroke K; // ��ȯ�� Stroke K ����
	K.r = R; // r�� �귯�� ������ ����
	K.color = cvGet2D(ref, y0, x0); // ref���� ���޹��� (max_x, max_y)�ȼ��� �÷��� ������ ����
	K.point[0][0] = x0; // Stroke�� �׸��� ������ ��ġ�� max_x�� ù��° ����Ʈ�� ����
	K.point[0][1] = y0; // Stroke�� �׸��� ������ ��ġ�� max_y�� ù��° ����Ʈ�� ����
	int y = y0; // gx, gy�� ����� ���� �� y�� max_y�� �ʱ�ȭ
	int x = x0; // gx, gy�� ����� ���� �� x�� max_x�� �ʱ�ȭ
	float lastDy = 0; // ���� ����Ʈ���� ���� dy�� ������ ���� lastDy�� 0���� �ʱ�ȭ
	float lastDx = 0; // ���� ����Ʈ���� ���� dx�� ������ ���� lastDx�� 0���� �ʱ�ȭ

	int i;
	for (i = 1; i < maxStrokeLength; i++)
	{
		// x, y�� 1���� �۰ų� ���� width-2, heigth-2���� Ŭ ��� �Լ� ����
		if (x < 1 || y < 1 || x > ref->width-2 || y > ref->height-2) {
			K.length = i;
			return K;
		}

		int r_cError = 0; // ref �� canvas ������ ����
		int r_sError = 0; // ref �� Stroke ������ ����

		// ref �� canvas ������ ���� ���
		for (int k = 0; k < 3; k++)
			r_cError += abs(cvGet2D(ref, y, x).val[k] - cvGet2D(canvas, y, x).val[k]);
		// ref �� canvas ������ ���� ���
		for (int k = 0; k < 3; k++)
			r_sError += abs(cvGet2D(ref, y, x).val[k] - K.color.val[k]);

		// i�� �ּ� ��Ʈ��ũ ���̺��� ��� ref�� canvas�� �������� ref�� stroke�� ������ Ŭ ��� �Լ� ����
		if (i > minStrokeLength && r_cError < r_sError) {
			K.length = i;
			return K;
		}

		float gy = 0; // y���� ���� ��ȭ�� gy
		float gx = 0; // x���� ���� ��ȭ�� gx

		// x, y�� �������� ��,��,��,���� �� ��ȭ���� ����Ͽ� y���� ���� ��ȭ��, x���� ���� ��ȭ���� ���� ����(gx,gy) ���ϱ�
		for (int k = 0; k < 3; k++)
			gx += cvGet2D(ref, y, x+1).val[k] - cvGet2D(ref, y, x-1).val[k];
		for (int k = 0; k < 3; k++)
			gy += cvGet2D(ref, y+1, x).val[k] - cvGet2D(ref, y-1, x).val[k];
		
		// gx�� gy�� ��� 0�� ��� �Լ� ����
		if (sqrt(gx * gx + gy * gy) == 0) {
			K.length = i;
			return K;
		}

		// ������ ���ϴ� ������ ���� (gx,gy)�� ������ ����(dx,dy)�� ���
		float dx = -gy;
		float dy = gx;

		// dx,dy�� ������ lastDx, lastDy�� ����� �ݴ��� ��� ������ �ݴ�� ����
		if (lastDx * dx + lastDy * dy < 0) {
			dx = -dx;
			dy = -dy;
		}

		// ��� fc�� ���� dx,dy �� lastDx,lastDy�� ������ ������ ���Ͽ� dx dy�� �����Ͽ� ��� ����
		dx = fc * dx + (1 - fc) * lastDx;
		dy = fc * dy + (1 - fc) * lastDy;

		// (dx,dy)�� �������ͷ� ����� ����
		dx = dx / sqrt(dx * dx + dy * dy);
		dy = dy / sqrt(dx * dx + dy * dy);

		//�귯�� ������ ��ŭ (dx,dy) �������� ����Ʈ �̵�
		x = x + R * dx;
		y = y + R * dy;

		//lastDx, lastDy�� dx dy ����
		lastDx = dx;
		lastDy = dy;

		// (x,y) �� K.point�� �߰��Ͽ� spline�� �� �� �߰�
		K.point[i][0] = x;
		K.point[i][1] = y;
	}
	K.length = i; // K.length�� i ����
	return K; // �ϼ��� Spline Stroke K ��ȯ
}

char* inputFileName()
{
	char* str = (char*)malloc(sizeof(char)); // ũ�Ⱑ 1�� char�� �迭 �����Ҵ�
	printf("================================\nMultimedia Programming Homework4\n<Painterly Rendering>\n21011778 Yeom Jihwan\n================================\n");
	printf("Input File Path: ");

	// ���ڸ� ���������� �Է¹����� ���ڰ� �þ �� ���� �迭�� �ٽ� �Ҵ��Ͽ� ����
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

	// ���ڿ� ��ȯ
	return str;
}