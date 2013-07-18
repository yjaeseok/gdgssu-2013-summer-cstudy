#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h> 
#include <fcntl.h>

char getch() 
{
        int ch; 

        struct termios buf;
        struct termios save;

        tcgetattr(0, &save);
        buf = save;

        buf.c_lflag &= ~(ICANON|ECHO);
        buf.c_cc[VMIN] = 1;
        buf.c_cc[VTIME] = 0;

        tcsetattr(0, TCSAFLUSH, &buf);

        ch = getchar();

        tcsetattr(0, TCSAFLUSH, &save);
        return ch; 
}

int kbhit()
{
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

        if(ch != EOF)
        {
                ungetc(ch, stdin);
                return 1;
        }

        return 0;
}

void gotoxy(int x, int y)
{
        printf("\033[%d;%df", y, x);
        fflush(stdout);
}

int max(int x, int y)
{
        if( x > y )
                return x;
        else
                return y;
}
                
#define LEFT 68
#define RIGHT 67
#define UP 65
#define DOWN 66
#define SPACE 32
#define BX 5
#define BY 1
#define BW 10
#define BH 20

#define TRUE 1
#define FALSE 0
typedef int BOOL;

void DrawBoard();                       // 게임판을 그린다.
BOOL ProcessKey();                      // 키 입력을 처리한다.
void PrintBrick(BOOL Show);             // 벽돌을 그리거나 지운다.
int GetAround(int x,int y,int b,int r); // 주변을 검사하여 회전 가능성을 판단한다.
BOOL MoveDown();                        // 아래로 1칸 이동한다, 맨 마지막 줄이면 TestFull을 호출한다.
void TestFull();                        // 수평으로 다 채워진 줄을 찾아서 지운다.

struct Point {
        int x;
        int y;
};

struct Point Shape[7][4][4]={
        { {{0,0},{1,0},{2,0},{-1,0}}, {{0,0},{0,1},{0,-1},{0,-2}},
                {{0,0},{1,0},{2,0},{-1,0}}, {{0,0},{0,1},{0,-1},{0,-2}} },
        { {{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
                {{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}} },
        { {{0,0},{-1,0},{0,-1},{1,-1}}, {{0,0},{0,1},{-1,0},{-1,-1}},
                {{0,0},{-1,0},{0,-1},{1,-1}}, {{0,0},{0,1},{-1,0},{-1,-1}} },
        { {{0,0},{-1,-1},{0,-1},{1,0}}, {{0,0},{-1,0},{-1,1},{0,-1}},
                {{0,0},{-1,-1},{0,-1},{1,0}}, {{0,0},{-1,0},{-1,1},{0,-1}} },
        { {{0,0},{-1,0},{1,0},{-1,-1}}, {{0,0},{0,-1},{0,1},{-1,1}},
                {{0,0},{-1,0},{1,0},{1,1}}, {{0,0},{0,-1},{0,1},{1,-1}} },
        { {{0,0},{1,0},{-1,0},{1,-1}}, {{0,0},{0,1},{0,-1},{-1,-1}},
                {{0,0},{1,0},{-1,0},{-1,1}}, {{0,0},{0,-1},{0,1},{1,1}} },
        { {{0,0},{-1,0},{1,0},{0,1}}, {{0,0},{0,-1},{0,1},{1,0}},
                {{0,0},{-1,0},{1,0},{0,-1}}, {{0,0},{-1,0},{0,-1},{0,1}} }
};

int EMPTY = 0;
int BRICK = 1;
int WALL = 2;
char *arTile[]={". ","■","□"};
int board[BW+2][BH+2];
int nx,ny;
int brick,rot;

int main()
{
        int nStay, nFrame, x, y;

        srand(time(NULL));
        system("clear");
        for (x=0;x<BW+2;x++) {
                for (y=0;y<BH+2;y++) {
                        if( y == 0 || y == BH+1 || x == 0 || x == BW+1 )
                                board[x][y] = WALL;
                        else
                                board[x][y] = EMPTY;
                        // 사이드 끝이면 WALL을, 사이드 끝이 아니면 공백을 넣는다.
                }   
        }   

        DrawBoard();
        nFrame=500;

        for (;1;) {
                brick=rand() % (sizeof(Shape)/sizeof(Shape[0]));
                nx=BW/2;
                ny=3;
                rot=0;
                PrintBrick(TRUE);                               // 벽돌을 출력

                if(GetAround(nx,ny,brick,rot) != EMPTY)         // 놓을 수 없는 상태이면 게임 오버
                        break;

                nStay=nFrame;
                for (;2;)
                {
                        if(--nStay == 0)                        // 200회를 모두 소진했다면 아래로 이동
                        {
                                nStay=nFrame;
                                if(MoveDown())                  // 벽돌이 마지막에 도착했으면 다음 벽돌을 불러온다.
                                        break;
                        }
                        if(ProcessKey())                        // 키가 입력되었고, 바닥에 도착했으면 다음 벽돌을 불러온다.
                                break;
                        usleep(1000/20);                        // 0.005 초 대기
                }
        }
        system("clear");
        gotoxy(30,12);
        puts("G A M E  O V E R");

        return 0;
}

void DrawBoard()
{
        int x,y;

        for (x=0;x<BW+2;x++) {
                for (y=0;y<BH+2;y++) {
                        gotoxy(BX+x*2,BY+y);
                        puts(arTile[board[x][y]]);
                }
        }

	gotoxy(BX, BH+3);
	puts("GDG SSU 2013 Summer C Study!");
}

BOOL ProcessKey()
{
        int ch,trot, y;

        if (kbhit()) {  // 키가 눌렸는지 확인
                ch = getch();

                if(ch == LEFT) {
                        if (GetAround(nx-1,ny,brick,rot) == EMPTY)
                        {
                                PrintBrick(FALSE);
                                nx--;
                                PrintBrick(TRUE);
                        }
                } else if(ch == RIGHT) {
                        if (GetAround(nx+1,ny,brick,rot) == EMPTY)
                        {
                                PrintBrick(FALSE);
                                nx++;
                                PrintBrick(TRUE);
                        }
                } else if(ch == UP) {
                        if(rot == 3)
                                trot = 0;
                        else
                                trot = rot + 1;
                        if(GetAround(nx,ny,brick,trot) == EMPTY)
                        {
                                PrintBrick(FALSE);
                                rot=trot;
                                PrintBrick(TRUE);
                        }
                } else if(ch == DOWN) {
                        if(MoveDown())
                        {
                                return TRUE;
                        }
                } else if(ch == SPACE) {
                        while(MoveDown()==FALSE)
                        {;}
                        return TRUE;
                }
        }
        return FALSE;
}

void PrintBrick(BOOL Show)
{
        int i, x, y;

        for (i=0;i<4;i++) {
                x = Shape[brick][rot][i].x;
                y = Shape[brick][rot][i].y;
                gotoxy(BX+(x+nx)*2,BY+y+ny);
                if(Show == TRUE)
                        puts(arTile[BRICK]);
                else
                        puts(arTile[EMPTY]);
        }
}

int GetAround(int x,int y,int brick,int rot)
{
        int i, sx, sy, k=EMPTY;

        for (i=0;i<4;i++) {
                sx = Shape[brick][rot][i].x;
                sy = Shape[brick][rot][i].y;
                k = max(k,board[x+sx][y+sy]);
        }
        return k;
}

BOOL MoveDown()
{
        if (GetAround(nx,ny+1,brick,rot) != EMPTY) {
                TestFull();
                return TRUE;
        }
        PrintBrick(FALSE);
        ny++;
        PrintBrick(TRUE);
        return FALSE;
}

void TestFull()
{
        int i, sx, sy, x, y, ty;

        for (i=0;i<4;i++) {
                sx = Shape[brick][rot][i].x;
                sy = Shape[brick][rot][i].y;
                board[nx+sx][ny+sy]=BRICK;
        }

        for (y=1;y<BH+1;y++) {
                for (x=1;x<BW+1;x++) {
                        if (board[x][y] != BRICK)
                                break;
                }
                if (x == BW+1) {
                        for(ty=y;ty>1;ty--)
                        {
                                for(x=1;x<BW+1;x++)
                                {
                                        board[x][ty]=board[x][ty-1];
                                }
                        }
                        DrawBoard();
                        usleep(200);
                }
        }
}

