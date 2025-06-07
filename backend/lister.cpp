#include "funcs.cpp"
#define MAXPAGE 3


int main(){
    int fd = init_read();
    int page = 0;
    int input=0;
    sendserial("9");
    std::string temp="";
    while (1) {
        input=readport(fd);
        if(input<1||input>5){

            continue;
        }
        if(input==5){
            std::cout<<page<<std::endl;
            page=(page+1)%MAXPAGE;
            continue;
        }
        executefunction(page, input);
    }
}
