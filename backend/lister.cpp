#if defined(_WIN32)
    #include "winfuncs.cpp"
    using SERIAL_HANDLE = HANDLE;
#elif defined(__linux__)
    #include "funcs.cpp"
    using SERIAL_HANDLE = int;
#else
    #error "Unsupported platform"
#endif


#define MAXPAGE 3


int main(){
    SERIAL_HANDLE fd = init_read();
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
            page=(page+1)%MAXPAGE;
            continue;
        }    
        executefunction(page, input);
    }
}
