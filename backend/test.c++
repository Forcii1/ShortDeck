
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h> 
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <iostream>

int init_read(){
    int fd = open("/dev/ttyACM0", O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }
    cfsetispeed(&tty, B115200);  // Set baud rate
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &tty);


    return fd;
}

std::string readport(int fd){
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    int ret = select(fd + 1, &readfds, nullptr, nullptr, &timeout);
    if (ret > 0 && FD_ISSET(fd, &readfds)) {
        char buffer[256];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            //std::cout << "Received: " << buffer << std::endl;
            //close(fd);
            return buffer;
        }
    } else if (ret == 0) {
        //std::cout << "Timeout: no data" << std::endl;
    } else {
        perror("select");
    }

    //close(fd);
    return "";
}
int sendserial(std::string send){
    std::ofstream serial("/dev/ttyACM0");
    if(!serial.is_open()){
        return 1;
    }
    serial<<send<<std::endl;
    serial.close();
    return 0;
}
int main(){
    int fd = init_read();
    sendserial("Hallo was geht");
    while (1) {
         std::cout<<readport(fd)<<"";
      if(readport(fd).compare("Hallo was geht")==0){
        std::cout<<"1\n";
      }
    }
}