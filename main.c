/*----------------------------------------------------------------------------
 *  簡易版シェル
 *--------------------------------------------------------------------------*/

/*
 *  インクルードファイル
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h> 
/*
 *  定数の定義
 */
#define MAXSTACK 1000
#define MAXALIAS 1000
#define BUFLEN    1024     /* コマンド用のバッファの大きさ */
#define MAXARGNUM  256     /* 最大の引数の数 */
char *alias_word[MAXALIAS][2];
char *stack_dir[MAXALIAS];
char *history[32];
int historyend=0;
int last_alias  = 0;
int last_stack = 0;
char word_command[256] = "Command :";
/*
 *  ローカルプロトタイプ宣言
 */
bool is_file(const char* path);
bool is_dir(const char* path);
int string_to_num(char* string);
int history_position(char arg[]);
int parse(char [], char *[]);
char* connect_args(char* args[],int command_status);
void save_history(char *args[],int command_status);
void execute_command(char *[], int);
void cd_command(char *args[]);
void pushd_command();
void dirs_command();
void popd_command();
void history_command(char *args[]);
void exc2_command(char *args[]);
void alias_command(char *args[]);
void unalias_command(char *args[]);
void prompt_command(char *args[]);
void pwd_command(char *args[]);
char* wildcard_command(char* args);
void script_command(char *args[]);
void rm_command(char *args[]);
void external_command(char *args[],int command_status);/*----------------------------------------------------------------------------
 *
 *  関数名   : main
 *
 *  作業内容 : シェルのプロンプトを実現する
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/
bool is_file(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISREG(buf.st_mode);
}

bool is_dir(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISDIR(buf.st_mode);
}
int main(int argc, char *argv[])
{
    char command_buffer[BUFLEN]; /* コマンド用のバッファ */
    char *args[MAXARGNUM];       /* 引数へのポインタの配列 */
    int command_status;          /* コマンドの状態を表す
                                    command_status = 0 : フォアグラウンドで実行
                                    command_status = 1 : バックグラウンドで実行
                                    command_status = 2 : シェルの終了
                                    command_status = 3 : 何もしない */
    alias_word[0][0] = NULL;
    alias_word[0][1] = NULL;
    /*
     *  無限にループする
     */

    for(;;) {

        /*
         *  プロンプトを表示する
         */
        printf("%s  ",word_command);
        /*
         *  標準入力から１行を command_buffer へ読み込む
         *  入力が何もなければ改行を出力してプロンプト表示へ戻る
         */
        char c,command_len=0;
        if(c==EOF) break;
        while((c=getchar())!=EOF){
          if(c=='\n' || c=='\0') {
            command_buffer[command_len]=' ';
            command_len++;
            command_buffer[command_len]='\0';
            command_len=0;
            break;
          }
          else{
          command_buffer[command_len]=c;
          command_len++;
          command_buffer[command_len]='\0';
          }
        }
        if(c==EOF) {
            command_buffer[command_len]=' ';
            command_len++;
            command_buffer[command_len]='\0';
            command_len=0;
          }
        printf("%s\n",command_buffer);
        /*
         *  入力されたバッファ内のコマンドを解析する
         *
         *  返り値はコマンドの状態
         */
        command_status = parse(command_buffer, args);
        /*
         *  終了コマンドならばプログラムを終了
         *  引数が何もなければプロンプト表示へ戻る
         */

        if(command_status == 2) {
            printf("done.\n");
            exit(EXIT_SUCCESS);
        } else if(command_status == 3) {
            continue;
        }
        /*
         *  コマンドを実行する
         */
        execute_command(args, command_status);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   : parse
 *
 *  作業内容 : バッファ内のコマンドと引数を解析する
 *
 *  引数     :
 *
 *  返り値   : コマンドの状態を表す :
 *                0 : フォアグラウンドで実行
 *                1 : バックグラウンドで実行
 *                2 : シェルの終了
 *                3 : 何もしない
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

int parse(char buffer[],        /* バッファ */
          char *args[])         /* 引数へのポインタ配列 */
{
    int arg_index;   /* 引数用のインデックス */
    int status;   /* コマンドの状態を表す */

    /*
     *  変数の初期化
     */

    arg_index = 0;
    status = 0;

    /*
     *  バッファ内の最後にある改行をヌル文字へ変更
     */

    *(buffer + (strlen(buffer) - 1)) = '\0';

    /*
     *  バッファが終了を表すコマンド（"exit"）ならば
     *  コマンドの状態を表す返り値を 2 に設定してリターンする
     */

    if(strcmp(buffer, "exit") == 0) {

        status = 2;
        return status;
    }

    /*
     *  バッファ内の文字がなくなるまで繰り返す
     *  （ヌル文字が出てくるまで繰り返す）
     */

    while(*buffer != '\0') {

        /*
         *  空白類（空白とタブ）をヌル文字に置き換える
         *  これによってバッファ内の各引数が分割される
         */
        while(*buffer == ' ' || *buffer == '\t') {
            *(buffer++) = '\0';
        }

        /*
         * 空白の後が終端文字であればループを抜ける
         */

        if(*buffer == '\0') {
            break;
        }

        /*
         *  空白部分は読み飛ばされたはず
         *  buffer は現在は arg_index + 1 個めの引数の先頭を指している
         *
         *  引数の先頭へのポインタを引数へのポインタ配列に格納する
         */

        args[arg_index] = buffer;
        ++arg_index;

        /*
         *  引数部分を読み飛ばす
         *  （ヌル文字でも空白類でもない場合に読み進める）
         */

        while((*buffer != '\0') && (*buffer != ' ') && (*buffer != '\t')) {
            ++buffer;
        }
    }

    /*
     *  最後の引数の次にはヌルへのポインタを格納する
     */

    args[arg_index] = NULL;

    /*
     *  最後の引数をチェックして "&" ならば
     *
     *  "&" を引数から削る
     *  コマンドの状態を表す status に 1 を設定する
     *
     *  そうでなければ status に 0 を設定する
     */

    if(arg_index > 0 && strcmp(args[arg_index - 1], "&") == 0) {

        --arg_index;
        args[arg_index] = '\0';
        status = 1;

    } else {
        status = 0;
    }

    /*
     *  引数が何もなかった場合
     */

    if(arg_index == 0) {
        status = 3;
    }

    /*
     *  コマンドの状態を返す
     */

    return status;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   : execute_command
 *
 *  作業内容 : 引数として与えられたコマンドを実行する
 *             コマンドの状態がフォアグラウンドならば、コマンドを
 *             実行している子プロセスの終了を待つ
 *             バックグラウンドならば子プロセスの終了を待たずに
 *             main 関数に返る（プロンプト表示に戻る）
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

void execute_command(char *args[],    /* 引数の配列 */ int command_status)     /* コマンドの状態 */{
    int status;   /* 子プロセスの終了ステータス */
    char *command[]  = { "cd", "pushd", "dirs", "popd", "history","!!","alias","unalias","prompt","pwd","rm",NULL};
      /*
      * check for command wildcard
      */
      for(int i=0;args[i]!=NULL;i++){
        if(strstr(args[i],"*")!=NULL){
          strcpy(args[i],wildcard_command(args[i]));
        }
      }
      char *command_buffer = connect_args(args,command_status);
      command_status = parse(command_buffer, args);
      /*
      *   check for command !!
      */ 
      if(strcmp(args[0],command[5])==0){
        char command_buffer[BUFLEN];
        strcpy(command_buffer,history[(historyend-1)%32]);
        command_status = parse(command_buffer, args);
      }
      /*
      *   check for command !string !n !-n
      */
      else if(*args[0]=='!'){
        char command_buffer[BUFLEN];
        int his_po = history_position(args[0]);
        if(his_po<0){
          printf("Past command according to the condition [%s] cannot be found\n",args[0]);
          return;
        }
        strcpy(command_buffer,history[his_po]);
        command_status = parse(command_buffer, args);
      }
      /*
      convert word from alias to be in command form
      */
      if(strcmp(args[0],"alias")!=0){
        for(int i=0;alias_word[i][0]!=NULL;i++){
          if(strcmp(alias_word[i][0],args[0])==0){ //if alias[0][i]=args[0]
              strcpy(args[0],alias_word[i][1]);
              break;
          }
        }
      }
      /*
      save command in history
      */
      save_history(args,command_status);
    /*char *command[] = { "cd", "pushd", "dirs", "popd", "history","!!","alias","unalias","prompt","pwd","rm",NULL};*/
    int command_no = -1;
    for(int i=0;command[i]!=NULL;i++){
      if(strcmp(args[0],command[i])==0) command_no= i;
    }
    switch(command_no){
      case 0: cd_command(args);break;
      case 1: pushd_command(args);break;
      case 2: dirs_command(args);break;
      case 3: popd_command(args);break;
      case 4: history_command(args);break;
      case 5: break;
      case 6: alias_command(args);break;
      case 7: unalias_command(args);break;
      case 8: prompt_command(args);break;
      case 9: pwd_command(args);break;
      case 10: rm_command(args);break;
      default: external_command(args,command_status);break;
    }
}

void external_command(char* args[],int command_status){
      /*
     *  子プロセスを生成する
     *  生成できたかを確認し、失敗ならばプログラムを終了する
     */
    /******** Your Program ********/
    /* プロセスＩＤ */ 
    int pid= fork(); //pid=0(child) >0(parent)
    if(pid < 0 ){ 
      fprintf(stderr,"ERROR: Failed to fork child process \n");
      _exit(0);
    }
    /*
     *  子プロセスの場合には引数として与えられたものを実行する
     *  引数の配列は以下を仮定している
     *  ・第１引数には実行されるプログラムを示す文字列が格納されている
     *  ・引数の配列はヌルポインタで終了している
     */
    /******** Your Program ********/
    if(pid==0) {// child process
        if(execvp(args[0],args)<0){
          fprintf(stderr,"[%s] command doesnt exist\n",args[0]);
        }
        _exit(0);
        return;
    }
    /*
     *  コマンドの状態がバックグラウンドなら関数を抜ける
     */
    /******** Your Program ********/
    if ( pid>0 && command_status == 1) { // background
      printf("background command &\n");
      return; //
    }
    /*
     *  ここにくるのはコマンドの状態がフォアグラウンドの場合
     *
     *  親プロセスの場合に子プロセスの終了を待つ
     */
    /******** Your Program ********/
    else if(pid>0 && command_status==0){ //now in parent process
    //wait() は、状況情報を取得した子の プロセス ID (PID) になっている値を戻
        if(wait(NULL)==-1){ //error in 
          fprintf(stderr,"ERROR\n");
          return;
      }
    }
}

int history_position(char arg[]){
  int current_position = (historyend-1+32)%32;
  char *string = arg;
  string++;
  /* Position for !string*/
  for(int i=0;i<32 && i<historyend;i++){
    if(strstr(history[current_position],string)!=NULL && (strstr(history[current_position],string)-history[current_position])==0){
      return current_position;
    }
    else{
      current_position-=1;
      if(current_position<0) current_position+=32;
    }
  }
  /* Position for !-n*/
  int num;
  if(*string=='-'){
    string++;
    num = string_to_num(string);
    if (num<0|| num>32) return -1;
    current_position = (historyend-num);
    if(current_position<0) return -1;
    return current_position%32; 
  }
  /* Position for !n*/
  else {
    num = string_to_num(string);
    if(num>32 || num>historyend) return -1;
    if(historyend<32 && num<=historyend) return num-1;
    return current_position = (historyend+num-1)%32;
  }
  return -1;
}

int string_to_num(char* string){
  int num =0;
  while(*string!='\0'){
      if(*string>='0' && *string<='9'){
        num*=10;
        num+=(*string-'0');
        string++;
      }
      else{
        return -1;
      }
  }
  return num;
}

char* connect_args(char* args[],int command_status){//
    char *combine_args = (char *)malloc(sizeof(char)*256);
    strcpy(combine_args,args[0]);
    for(int i=1;args[i]!=NULL;i++){
        strcat(combine_args," ");
        strcat(combine_args,args[i]);
    }
    if(command_status==1){
      strcat(combine_args," ");
        strcat(combine_args,"&");
    }
    strcat(combine_args," ");
    return combine_args;
}
void save_history(char *args[],int command_status){
    char *buf_history = connect_args(args,command_status);
    /* this line need confirmation : how to use free correctly */
    //free(history[historyend%32]);
    history[historyend%32] = buf_history;
    historyend++;
}

void cd_command(char *args[]){
  char cwd[256];
  if(args[2]!=NULL && args[1]!=NULL){
    printf("Too much data for command [cd]\n");
    return;
  }
  else{
    if(args[1]==NULL){
        if(getenv("HOME")==NULL || chdir(getenv("HOME"))==-1){
          printf("The location is not valid\n");
          return;
        }
    }
    else if(chdir(args[1])==-1){
      printf("The location is not valid\n");
      return;
    }
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current Directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }
  }
}

void pushd_command(char *args[]){
  char cwd[256];
  if(args[1]!=NULL){
    printf("Too much data for command [pushd]\n");
    return;
  }
  if(last_stack==MAXSTACK){
    printf("The stack is full\n");
  }
  else{
      char *stack_word1 = (char*)malloc(sizeof(char)*(256));
      strcpy(stack_word1, getcwd(NULL, 0));
      stack_dir[last_stack] = stack_word1;
      last_stack++;
    }
}

void dirs_command(char *args[]){
  if(args[1]!=NULL){
    printf("Too much data for command [dirs]\n");
  }
  printf("Current Stack(from newest): \n");
        for(int i=0;i<last_stack;i++){
          printf("\t%s\n",stack_dir[last_stack-1-i]);
        }
}

void popd_command(char *args[]){
  char cwd[256];
  if(args[1]!=NULL){
    printf("Too much data for command [popd]\n");
    return;
  }
  if(last_stack<1){
    printf("No directory in stack\n");
    return;
  }
  last_stack--;
  if(chdir(stack_dir[last_stack])==-1){
      printf("The uppermost directory in slack is not valid\n");
  }
  /* this line need confirmation : how to use free correctly */
  free(stack_dir[last_stack]);
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current Directory: %s\n", cwd);
  }
  else {
        perror("getcwd() error\n");
  }
  return;
}
void history_command(char *args[]){

  int start=0;
  if(args[1]!=NULL){
      printf("Too much data for command [history] \n");
      return;
  }
  if(historyend>=32) start = historyend%32; //historyend=32(0-31) 33(1-0)
  printf("History:\n");
  for(int i=0;start+i<historyend && i<32;i++){
      printf("(%d) %s\n",i+1,history[(start+i)%32]);
  }
  return;
}

void alias_command(char *args[]){
    if(args[1]==NULL){
      for(int i=0;i<last_alias;i++)
      {
        printf("%s %s\n",alias_word[i][0],alias_word[i][1]);
      }
      return;
    }
    else if(args[2]==NULL){
      printf("Insufficient data for command [alias] \n");
      return;
    }
    else if(args[3]!=NULL){
      printf("Too much data for command [alias] \n");
      return;
    }
    else{
      if(last_alias==MAXALIAS){
        printf("The alias storage is full: delete some aliases first\n");
        return;
      }
      else{
      for(int i=0;i<last_alias;i++)
      {
        if(strcmp(alias_word[i][0],args[1])==0){
          strcpy(alias_word[i][1],args[2]);
          return;
        }
      }
      char *alias_word1 = (char*)malloc(sizeof(char)*(strlen(args[1])));
      char *alias_word2 = (char*)malloc(sizeof(char)*(strlen(args[2])));
      strcpy(alias_word1, args[1]);
      strcpy(alias_word2, args[2]);
      alias_word[last_alias][0]=alias_word1;
      alias_word[last_alias][1]=alias_word2;
      last_alias++;
      alias_word[last_alias][0]=NULL;
      alias_word[last_alias][1]=NULL;
      return;
      }
    }
    return;
}
void unalias_command(char *args[]){
  if(args[1]==NULL){
      printf("Insufficient data for command [unalias] \n");
      return;
    }
    else if(args[2]!=NULL){
      printf("Too much data for command [unalias] \n");
      return;
    }
    else{
      int i=0;
      for(1;i<last_alias && strcmp(args[1],alias_word[i][0])!=0;i++);
      if(i<last_alias){
        /* this line need confirmation : how to use free correctly */
        free(alias_word[i][0]);
        while(i<last_alias){
          alias_word[i][0]=alias_word[i+1][0];
          alias_word[i][1]=alias_word[i+1][1];
          i++;
        }
       last_alias--;
      }
     else{
       printf("Alias [%s] not found\n",args[1]);
     }
    }
}
void prompt_command(char *args[]){
  if(args[1]!=NULL && args[2]!=NULL){
      printf("Too much data for command [prompt]\n");
      return;
    }
  else if(args[1]==NULL){
      strcpy(word_command,"Command :");
      return;
  }
  strcpy(word_command,args[1]); 
}

void pwd_command(char *args[]){
  char cwd[256];
  /*if(args[1]!=NULL){
    printf("Too much data for command [pwd]\n");
    return;
  }*/
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current Directory: %s\n", cwd);
  }
}

char* wildcard_command(char *args){
  char* new_args= (char *)malloc(sizeof(char)*256);
  int position = -1; // 0 = * 1= *string 2 = string*
    if(strcmp(args,"*")==0){
            position = 0;
          }
    else if(strstr(args,"*")-args==0){
      position = 1;
      args++;
    }
    else if(strstr(args,"*")!=NULL && strstr(args,"*")-args!=0){
      position = 2;
      int i=0;
      for(i = 0;*(args+i)!='\0';i++);
      i--;
      args[i]='\0';
    }
    else{
      return " ";
    }
    struct stat    filestat;
    struct dirent *directory;
    DIR           *dp;
    dp = opendir(".");
    /*** [ディレクトリエントリの取得] ***/
    while((directory = readdir(dp))!=NULL){
      if(!strcmp(directory->d_name, ".") ||
        !strcmp(directory->d_name, ".."))
        continue;
    /*** [i-node 情報(ファイル情報)の取得] ***/
      if(stat(directory->d_name,&filestat)<0){
        perror("main");
        exit(1);
      }else{
        /* 表示 */
        if(strstr(directory->d_name,".")-directory->d_name==0) continue;
        if(position==0){
            strcat(new_args,directory->d_name);
            strcat(new_args," ");
        }else if(position==1 && strstr(directory->d_name,args)-directory->d_name+strlen(args)==strlen(directory->d_name)){
          strcat(new_args,directory->d_name);
          strcat(new_args," ");
        } else if(position==2 && strstr(directory->d_name,args)-directory->d_name==0){
          strcat(new_args,directory->d_name);
          strcat(new_args," ");
        }
      }
    }
    return new_args;
}
void rm_command(char *args[]){
  if(args[1]==NULL || (args[1][0]=='-' && args[1][1]=='r' && args[1][2]=='\0' && args[2]==NULL)){
    printf("Insufficient data for command [rm] \n");
    return;
  }
  int start;
  if(args[1][0]=='-' && args[1][1]=='r' && args[1][2]=='\0') start = 2;
  else{
    start =1;
  }
  for(int i=start;args[i]!=NULL;i++){
    if(is_file(args[i])==true || (is_dir(args[i])==true && start==2)){
      if (remove(args[i]) == 0) {
          printf(" [%s] is deleted successfully\n",args[i]);
      } else {
          printf("[%s] is not deleted\n",args[i]);
      }
    }
    else if(is_dir(args[i])==true && start==1){
      printf("Directory [%s] cannot be deleted\n",args[i]);
    }
  }
  return;
}