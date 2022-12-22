#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include "config.h"
#include "functionDefine.h"
#include <cstring>
using namespace std;

typedef struct SUPER_BLOCK super_block;
super_block superblock;

fstream control;
fstream user;
fstream disk;
ofstream log;
std::time_t t;

char curName[128] = "/";
int road[100];
int num;

void openDisk() {
    disk.open(diskFile, ios::in | ios::out);
}

void openUser() {
    user.open(userFile, ios::in | ios::out);
}
void closeUser() {
    user.close();
}
void closeDisk() {
    disk.close();
}

void insertLog(char * logContent) {
    log.open(operatorLogFile, ios::in | ios::out | ios::app);
    log << *logContent << endl;
    log.close();
}

string getTime() {
    char tempBuf[9];
    t = std::time(NULL);
    std::strftime(tempBuf, sizeof(tempBuf), "%H:%M:%S", std::localtime(&t));
//    cout << tempBuf;
    return tempBuf;
}

/**
 * 输入日志模块
 * @param command
 * @param input
 */
void getLog(char * command, char * input ) {
    int commandLen = (int)strlen(command);
    int inputLen = (int)strlen(input);
    char operationLog[commandLen + inputLen + 20];
    strcpy(operationLog, command);
    strcat(operationLog, "\t");
    strcat(operationLog, input);
    strcat(operationLog, "\t");
    strcat(operationLog, getTime().c_str());
    insertLog(operationLog);
}

int main() {
    // Windwos下面设定终端输出的编码格式为GBK
    system("chcp 65001 > nul");
    control.open(controlFile, ios::in | ios::out | ios::app);
    int i = -1;
    control >> i;
    control.close();
    if (i != 0) {
        /**
         * 不为0就初始化
         */
        initial();
    }
    control.open(controlFile, ios::in | ios::out);
    control.seekg(0);
    control << 0;
    control.close();
    strcpy(curName, "root");
    road[0] = 0;
    num= 1;

    cout << "请登录系统" << endl;
    while (!login()) {
        cout << "错误" << endl;
    }
    cout << "登录成功 ！！！\n";
    readsuper();
    getCommand();
    writesuper();
}

void initial() {
    cout << "初始化......" << endl;
    // 用户组的初始化， 最多用户名5位，密码5位，用户组5位
    user.open(userFile, ios::in | ios::out | ios::trunc);
    // 用户名
    user << setw(6) << "adm";
    // 密码
    user << setw(6) << "123";
    // 用户组
    user << setw(6) << "adm";
    user << setw(6) << "cnj";
    user << setw(6) << "123";
    user << setw(6) << "adm";
    user << setw(6) << "jtq";
    user << setw(6) << "123";
    user << setw(6) << "guest";
    user.close();
    // disk初始化
    disk.open(diskFile, ios::in | ios::out | ios::trunc);
    if (!disk) {
        cout << "磁盘不可用" << endl;
        exit(1);
    }
    int j;
    for (j = 0; j < 100; ++j) {
        disk << setw(512) << "";
        disk << '\n';
    }
    // 初始化超级块， 0# 盘块
    // 空闲节点号栈
    disk.seekp(0);
    disk<< setw(3) << -1;
    for (j = 1; j < 79; ++j) {
        disk << setw(3) << j;
    }
    // 空闲节点指针 == 当前节点数（240-242）
    disk << setw(3) << 79;
    for (j = 0; j < 10; ++j) {
        // 空闲盘块号栈 10 * 3 = 30B 11 # 盘块已使用末尾盘块为21
        disk << setw(3) << j + 12;
    }
    disk << setw(3) << 10;    // 空闲节点栈指针==当前栈的盘块数
    disk << setw(3) << 80;    // 空闲节点总数
    disk << setw(3) << 89;    // 空闲盘块总数
    disk.seekp(514);             // 根目录文件的初始化 1#盘块开始的
    disk << setw(6) << 0;     // 文件大小
    disk << setw(6) << 1;     // 文件盘块数
    disk << setw(3) << 11;    //
    disk << setw(3) << 0;     // 指向0#表示没有指向
    disk << setw(3) << 0;     //
    disk << setw(3) << 0;     //
    // 一个一次间址
    disk << setw(3) << 0;
    disk << setw(3) << 0;     // 一个两次间址
    disk << setw(6) << "adm";  // 文件所有者
    disk << setw(6) << "adm";   // 文件所属组
    disk << setw(11) << "drwxrwxrwx";   // 文件类型及存储权限
    const string &time = getTime();
    // 最近修改时间
    disk << setw(9) << time;
    /**
     * 根目录初始化
     * 如果尚无子目录，不用初始化
     */
    /**
     * 空闲盘块初始化，只有初始化记录盘块 i#
     * 成组链接最后记录盘块最后记录号后要压个0
     */
    for(j = 21; j < 100; j++) {
        if (j % 10 == 1) {
            // 定位记录盘块
            disk.seekp(514 * j);
            for (int i = 0; i < 10; ++i) {
                int temp = i + j + 1;
                if (temp < 100) {
                    disk << setw(3) << temp;
                }
            }
        }
    }
    // 最后记录盘块需要压个0
    disk << setw(3) << 0;
    disk.close();
}

/**
 * 申请一个节点，返回节点号，否则返回-1
 * 返回的是空闲节点号栈中最小的节点号，
 * 节点号用完时返回-1，
 *
 * 申请失败
 * @return
 */
int ialloc() {
    if (superblock.fiptr > 0) {
        // 当前可用  80 - superblock.fiptr 80 是索引节点总个数，fiptr是空闲节点的个数
        int temp = superblock.fistack[80 - superblock.fiptr];
        superblock.fistack[80 - superblock.fiptr] = -1;
        superblock.fiptr--;
        return temp;
    }
    return -1;
}

/**
 * 归还节点
 * 指定一个节点号，回收一个节点。先清空空节点，
 * 然后插入栈中合适节点，保持节点的有序性
 * @param index
 */
void ifree(int index) {
    openDisk();
    // 清空节点， 节点的索引定位 514 是0#, 64是索引大小，
    disk.seekp(514 + 64 * index + 2 * ( index / 8));
    disk << setw(6) << "";
    closeDisk();
    // 把节点号找到合适的位置插入到空闲节点栈
    for (int i = 80 - superblock.fiptr; i < 80; ++i) {
        // 若小于它， 前移一位
        if (superblock.fistack[i] < index) {
            superblock.fistack[i - 1] - superblock.fistack[i];
        } else {
            // 放在第1个大于它的节点号前面
            superblock.fistack[i - 1] = index;
            break;
        }
    }
    superblock.fiptr++;

}

/**
 * 读指定节点的索引节点信息（节点为index，读指针应该定位到514+64*index+2*(index/8),
 * 把索引节点信息保存到变量inode中，便于对同一节点进行大量操作
 * @param index
 * @param inode
 */
void readinode(int index, INODE & inode) {

    openDisk();
    disk.seekp(514 + 64*index + 2 * (index / 8));
    // 文件大小
    disk >> inode.fsize;
    // 文件盘块数
    disk >> inode.fbnum;
    // 4个直接盘块号
    for (int i = 0; i < 4; ++i) {
        disk >> inode.addr[i];
    }
    // 一个一次间址
    disk >> inode.addr1;
    // 一个两次间址
    disk >> inode.addr2;
    // 文件所有者
    disk >> inode.owner;
    // 文件所属组
    disk >> inode.group;
    // 文件类别及存储权限
    disk >> inode.mode;
    // 最近修改时间
    disk >> inode.ctime;
    closeDisk();
}

/**
 * 写节点 把inode写回指定的节点
 * @param inode
 * @param index
 */
void writenode(INODE inode, int index) {
    openDisk();
    disk.seekp(514 + 64*index + 2 * (index / 8));
    // 文件大小
    disk << setw(6) << inode.fsize;
    // 文件盘块数
    disk << setw(6) << inode.fbnum;
    for (int i = 0; i < 4; ++i) {
        disk << setw(3) << inode.addr[i];
    }
    // 一个一次间址
    disk << setw(3) << inode.addr1;
    // 一个两次间址
    disk << setw(3) <<inode.addr2;
    // 文件所有者
    disk << setw(6) << inode.owner;
    // 文件所属组
    disk << setw(6) << inode.group;
    // 文件类别及存储权限
    disk << setw(12) << inode.mode;
    // 最近修改时间
    disk << setw(10) << inode.ctime;
    closeDisk();
}

/**
 * 申请一个盘块，返回盘块号，否则返回-1
 * @return
 */
int balloc() {
    int temp = superblock.fbstack[10 - superblock.fbptr];
    // 到栈底了
    if (superblock.fbptr == 1) {
        // 最后记录盘块号0 （保留为栈底，分配不成功）
        if (temp == 0) {
            return -1;
        }
        superblock.fbstack[10 - superblock.fbptr] = -1;
        superblock.fbptr = 0;
        // 读取盘块内容读入栈

        int id, num = 0;
        openDisk();
        // 计算盘块内容个数num（最多10）最后盘块可能不到10个
        disk.seekp(514 * temp);
        for (int j = 0; j < 10; ++j) {
            disk >> id;
            num ++;
            if (id == 0) {
                break;
            }
        }
        disk.seekp(514 * temp);
        for (int j = 10 - num; j < 10; ++j) {
            disk >> id;
            superblock.fbstack[j] = id;
        }
        superblock.fbptr = num;
        closeDisk();
        // 清空回收盘块
        openDisk();
        disk.seekp(514 * temp);
        disk << setw(512) << "";
        closeDisk();
        return temp;
    } else {
        // 不是记录盘块
        superblock.fbstack[10 - superblock.fbptr] = -1;
        superblock.fbptr--;
        return temp;
    }
}

/**
 * 归还盘块
 * @param index
 */
void bfree(int index) {
    openDisk();
    // 清空该回收盘
    disk.seekp(514 * index);
    disk << setw(512) << "";
    closeDisk();
    // 如果栈已经满了的话，栈中内容计入回收盘块，栈清空
    if (superblock.fbptr == 10) {
        openDisk();
        disk.seekp(514* index);
        for (int i = 0; i < 10; ++i) {
            disk << setw(3) << superblock.fbstack[i];
            superblock.fbstack[i] = -1;
        }
        closeDisk();
        superblock.fbptr = 0;
    }
    // 回收盘块压栈
    superblock.fbstack[10 - superblock.fbptr - 1] = index;
    superblock.fbptr++;
}

/**
 * 读超级块到主存区
 */
void readsuper() {
    openDisk();
    int i;
    for (i = 0; i < 80; i++) {
        // 读取空闲节点栈
        disk >> superblock.fistack[i];
    }
    // 空闲栈指针
    disk >> superblock.fiptr;
    for (i = 0; i < 10; i++) {
        // 空闲盘号栈
        disk >> superblock.fbstack[i];
    }
    // 空闲盘号栈指针
    disk >> superblock.fbptr;
    // 空闲索引节点数量
    disk >> superblock.inum;
    // 空闲盘块号总数
    disk >> superblock.bnum;
    closeDisk();
}

/**
 * 写超级块
 */
void writesuper() {
    // 主存写回超级块
    openDisk();
    int i;
    for (i = 0; i < 80; i++) {
        // 写空闲节点栈
        disk << setw(3) << superblock.fistack[i];
    }
    // 空闲节点栈指针
    disk << setw(3) << superblock.fiptr;
    for (i = 0; i < 10; i++) {
        // 空闲盘块栈
        disk << setw(3) << superblock.fbstack[i];
    }
    disk << setw(3) << superblock.fbptr;
    // 空闲节点号
    disk << setw(3) << superblock.inum;
    // 空闲盘块号
    disk << setw(3) << superblock.bnum;
    closeDisk();
}

/**
 * 读目录
 * @param inode
 * @param index
 * @param dir
 */
void readdir(INODE inode, int index, DIR & dir) {

    openDisk();
    disk.seekp(514 * inode.addr[0] + 36 * index);
    disk >> dir.fname;
    disk >> dir.index;
    disk >> dir.parfname;
    disk >> dir.parindex;

    disk.seekp(514 * inode.addr[0] + 36 * index);
    disk << setw(15) << "";
    disk << setw(3) << "";
    disk << setw(15) << "";
    disk << setw(3) << "";
    closeDisk();
}

/**
 * 写目录到指定目录
 * @param inode
 * @param dir
 * @param index
 */
void writedir(INODE inode, DIR dir, int index) {
    openDisk();
    disk << setw(15) << dir.fname;
    disk << setw(3) << dir.index;
    disk << setw(15) << dir.parfname;
    disk << setw(3) << dir.parfname;
    closeDisk();
}

/**
 * 创建文件
 * @param filename
 * @param content
 */
void mk(char * filename, char * content) {
    /*
     * 在当前目录下创建一个数据文件（规定目录文件只占1—4盘块）。第1个参数表示文件名，
     * 第2个参数表示文件内容，可以在文件副本中使用这个函数
     */
    INODE inode, inode2;
    // 把当前节点road[num-1]内容读入索引节点
    readinode(road[num - 1], inode);
    if (havePower(inode)) {
        // 判断是否目录项已经达到14个
        if (512 - inode.fsize < 36) {
            cout << "当前目录已满，创建子目录失败";
        } else {
            int i, index2;
            // 判断是否有重名文件存在
            if (havesame(filename, inode, i , index2)) {
                cout << "该名已经存在，创建失败!";
            } else {
                int size = strlen(content) + 1;
                if (size > 2048) {
                    cout << "\n文件太大，创建失败";
                } else {
                    // 计算盘块数
                    int bnum = (size - 1) / 512 + 1;
                    int bid[4];
                    // 申请一个node
                    int iid = ialloc();
                    if (iid != -1) {
                        bool success = true;
                        // 申请盘块
                        for (int j = 0; j < bnum; ++j) {
                            bid[j] = balloc();
                            if (bid[j] == -1) {
                                cout << "盘块不够，创建数据文件失败!" << endl;
                                success = false;
                                // 释放已经申请到的inode和盘块
                                ifree(iid);
                                for (int k = j - 1; k >= 0; k--) {
                                    bfree(bid[j]);
                                }
                                break;
                            }
                        }
                        if (success) {
                            // 当前目录盘的修改
                            openDisk();
                            disk.seekp(514 * inode.addr[0] + inode.fsize);
                            // 写目录名
                            disk << setw(15) << filename;
                            disk << setw(3) << iid;
                            disk << setw(15) << curName;
                            disk << setw(3) << road[num - 1];
                            closeDisk();
                            // 当前目录节点的修改
                            inode.fsize += 36;
                            const string &time = getTime();
                            strcpy(inode.ctime, time.c_str());
                            // 新建目录节点的初始化
                            // 文件大小
                            inode2.fsize = size;
                            // 文件盘块数
                            inode2.fbnum = bnum;
                            int i;
                            for (i = 0; i < 4; ++i) {
                                if (i < bnum) {
                                    inode2.addr[i] = bid[i];
                                } else {
                                    inode2.addr[i] = 0;
                                }
                            }
                            // 一个一次间址
                            inode2.addr1 = 0;
                            // 一个两次间址
                            inode2.addr2 = 0;
                            // TODO 这个地方可能需要修改一下
                            strcpy(inode2.owner, curUserName);
                            strcpy(inode2.group, curgroup);
                            // 存储权限
                            strcpy(inode2.mode, "-rwxrwxrwx");
                            const string &time1 = getTime();
                            strcpy(inode2.ctime, time1.c_str());
                            writenode(inode2, iid);
                            // 新建文件盘块的初始化
                            char temp;
                            openDisk();
                            disk.seekp(514 * bid[0]);
                            for (i = 0; i < size; i++) {
                                temp = content[i];
                                disk << temp;
                                if (i / 512 == 511) {
                                    disk << '\n';
                                }
                            }
                            closeDisk();
                            cout << "文件已成功创建";
                        }
                    } else {
                        cout << "节点已用完， 创建数据文件失败!" << endl;
                    }
                }
            }
        }
    } else {
        cout << "你没有权限";
    }
    // 把inode写入到指定节点
    writenode(inode, road[num - 1]);
}

/**
 * 删除文件
 * @param filename
 */
void rm(char * filename) {
    // 当前目录下删除指定数据文件
    INODE inode, inode2;
    DIR dir;
    // 将当前节点写入节点对象
    readinode(road[num - 1], inode);
    // 权限
    if (havePower(inode)) {
        // int i, index2, i为待删除子目录目录项下标index2为目录项中的待删子目录的节点号
        int i, index2;
        if (havesame(filename, inode, i, index2)) {
            // 待删除子目录的节点写入节点对象
            readinode(index2, inode2);
            if (havePower(inode2)) {
                // 判断是否是数据文件而非目录文件
                if (inode2.mode[0] == '-') {
                    // 回收盘块和节点
                    for (int ii = 0; ii < inode2.fbnum; ++ii) {
                        bfree(inode2.addr[ii]);
                    }
                    ifree(index2);
                    // 对当前目录盘块的修改，inode.addr[0]为当前盘块号，i为待删子目录的目录项下标
                    openDisk();
                    // 清空当前盘块第i个子目录的目录项
                    disk.seekp(514 * inode.addr[0] + 36*i);
                    disk << setw(36) << "";
                    closeDisk();
                    // 后面的移一位
                    for (int j = i + 1; j < (inode.fsize / 36); ++j) {
                        // 读入指定盘块
                        readdir(inode, j, dir);
                        writedir(inode, dir, j - 1);
                    }
                    // 对当前目录节点的修改
                    inode.fsize -= 36;
                    const string &time = getTime();
                    strcpy(inode.ctime, time.c_str());
                    cout << "文件已经成功删除" << endl;
                } else {
                    cout << "目录文件应用rmdir命令删除";
                }
            } else {
                cout << "你没有权限！";
            };
        } else {
            cout << "目录中不存在该子目录";
        }
    } else {
        cout << "你没有权限" << endl;
    }
    writenode(inode, road[num - 1]);
}

/**
 * 删除文件
 * @author zhanglianyong
 * @param filename
 */
void rmForDir(char * filename, int index) {

    // 在调用这个函数之前，先把路径给存进去，然后再去除掉
    road[num] = index;
    num++;
    rm(filename);
    // 恢复到原先的路径下
    num--;
    road[num] = 0;
}

/**
 * 复制文件
 * @param filename
 */
void cp(char * string) {
    /*把指定目录下的指定文件复制到当前目录下*/
    //记录当前指定文件是否找到
    bool getit = false;
    //保存内容
    char content[2048];
    //保存文件名
    char fname[14];
    //保存当前路径
    char tcurname[14];
    int troad[20];
    int tnum = num;
    strcpy(tcurname, curName);
    for (int i = 0; i < num; i++)
    {
        troad[i] = road[i];
    }
    if (find(string))
    {
        INODE inode;
        readinode(road[num-1],inode);
        if(inode.mode[0] == '-')  /*确定是数据文件*/
        {
            getit = true;
            /*文件名复制到fname变量，盘块内容复制到字符串content[2048]*/
            strcpy(fname,curName);
            char temp;
            openDisk();
            int i,j;
            for(i = 0 ,j = 0 ; j < inode.fsize ; i++ , j++)
            {
                disk.seekg(514 * inode.addr[0]+i);
                disk>>temp;
                content[j] = temp;
                if(i/512 == 511)  //跳过'\n'
                {
                    i = i+2;
                }
            }
            content[j+1]='\0';
            closeDisk();
        }else{
            cout<<"不能根据路径找到相关目录";
        }
    }else{
        cout<<"不能根据路径找到相关目录";
    }
    strcpy(curName,tcurname);
    num = tnum;
    for (int ii = 0; ii < tnum ; ii++)
    {
        road[ii] = troad[ii];
    }
    if (getit)
    {
        mk(fname,content);
        cout<<"文件复制完毕";
    }
}

/**
 * 显示文件内容
 * @param filename
 */
void cat(char * filename) {
    /*显示文件内容（当前目录下指定数据文件）*/
    INODE inode,inode2;
    readinode(road[num-1],inode);  //当前节点写入节点对象
    int i,index2; //i为子目录待显目录项下标,index2为目录项中的待显示目录的节点号
    if(havesame(filename,inode,i,index2)){
        readinode(index2,inode2);  //要显示的数据文件的节点写入节点对象
        if(inode2.mode[0]== '-'){
            cout<<"文件内容为：（用文件名和*填充来模拟文件内容）\n";
            char content[512];
            openDisk();
            for (int i = 0; i < inode2.fbnum; i++)
            {
                disk.seekg(inode2.addr[i]*514);
                disk >> content;
                cout << content;
            }
            closeDisk();
        }else{
            cout << "显示失败（目标是目录文件，cat是用来显示数据文件内容的）";
        }
    }else {
        cout << "目录中不存在该数据文件！！！";
    }
}

/**
 * 判断目录项是否存在
 * @param dirname
 * @param inode
 * @param i
 * @param index2
 * @return
 */
bool havesame(char *dirname, INODE inode, int & i, int &index2) {
    /*
     * 判断对象inode指向的目录文件盘块中有无该名（dirname)的目录项存在，有则返回1，无则返回0.同时，若有该目录项，
     * 则按引用调用的i的待删子目录目录项下标，index2为目录项中待删子目录的节点号
     */
    bool have = false;
    char name[14];
    openDisk();
    // 遍历目录项
    for (i = 0; i < (inode.fsize / 36); ++i) {
        disk.seekp(514 * inode.addr[0] + 36*i);
        disk >> name;
        if (!strcmp(dirname, name)) {
            have = true;
            disk >> index2;
            break;
        }
    }
    closeDisk();
    return have;
}

/*查找文件或目录*/
bool find(char * string) {
    int ptr = 0;
    char name[14]=" ";
    INODE inode;
    /*读根目录*/
    for (int i = 0;string[ptr]!='/';ptr++, i++)
    {
        if(i==15) return 0;    //超过正常长度，返回0
        name[i] = string[ptr];
    }
    if(!strcmp(name,"root")){
        strcpy(curName,"root");
        road[0] = 0;
        num = 1;
        for(;;) {
            readinode(road[num-1],inode);
            ptr++;
            char name[14]=" ";
            for(int i=0;(string[ptr]!='/')&&(string[ptr]!='\0');ptr++,i++)
            {
                //从string读入一个名字
                if(i==15) return 0;
                name[i]=string[ptr];
            }
            //当前目录查找该目录项
            int ii,index2;
            if (havesame(name,inode,ii,index2)){
                char tname[14];
                openDisk();
                disk.seekg(514*inode.addr[0]+36*ii);
                disk >> tname;
                closeDisk();
                strcpy(curName, tname);
                road[num] = index2;
                num++;
                //判断字符串结尾
                if(string[ptr] == '\0')
                {
                    return 1;
                }
            }else {
                return 0;
            }

        }
    }else {
        return 0;
    }
}

/*
创建目录
*/
void mkdir(char * dirname) {
    //创建目录（规定目录文件只占一个盘块）当前目录下创建
    INODE inode,inode2;
    readinode(road[num-1],inode); //把当前节点road[num-1]的内容读入索引节点
    if(havePower(inode)) //判断权限
    {
        if(512-inode.fsize < 36) {  //判断目录是否已满
            cout << "当前目录已满，创建子目录失败！";
        }
        else {
            int i,index2;
            if (havesame(dirname,inode,i,index2)) {
                cout<<"该目录已存在，创建失败！";
            } else {
                int iid = ialloc();  //申请节点
                if(iid!= -1){
                    int bid = balloc(); //申请盘块
                    if(bid!= -1){
                        openDisk();
                        disk.seekg(514*inode.addr[0]+inode.fsize);
                        disk << setw(15) <<dirname; //写目录名
                        disk << setw(3) << iid;    //写节点
                        disk << setw(15) << curName;
                        disk << setw(3) << road[num-1];
                        closeDisk();
                        //当前目录节点的修改
                        inode.fsize +=36;
                        const string &time1 = getTime();
                        strcpy(inode.ctime,time1.c_str());
                        //新建目录节点的初始化
                        inode2.fsize = 0; //文件大小
                        inode2.fbnum = 1; //文件盘块数
                        inode2.addr[0] = bid;
                        for(int aaa = 1;aaa<4;aaa++) //指向0#表示没有指向
                        {
                            inode2.addr[aaa]=0;
                        }
                        inode2.addr1 = 0;//一个一次间址
                        inode2.addr2 = 0; //一个两次间址
                        strcpy(inode2.owner,curUserName); //文件所有者
                        strcpy(inode2.group,curgroup); //文件所属组
                        strcpy(inode2.mode,"drwxrwxrwx");
                        //文件类别及存储权限（默认最高）
                        const string &time2 = getTime();
                        //最近修改时间
                        strcpy(inode.ctime,time2.c_str());
                        writenode(inode2,iid);
                        openDisk();
                        disk.seekg(514+64*iid+2*(iid/8));
                        disk << setw(6) <<0; //文件大小
                        disk << setw(6) <<1;//文件盘块数
                        disk << setw(3) <<bid;
                        disk << setw(3) <<0;
                        disk << setw(3) <<0;
                        disk << setw(3) <<0;
                        disk << setw(3) <<0;
                        disk << setw(3) <<0;
                        disk << setw(6) << curUserName;
                        disk << setw(6) << curgroup;
                        disk << setw(12) <<"drwxrwxrwx";
                        //文件类别及存储权限
                        const string &time3 = getTime();
                        disk << setw(10) << time3;
                        closeDisk();
                        cout << "目录已成功创建";
                    }else{
                        ifree(iid); //释放刚申请的节点
                        cout << "盘块已用完，创建子目录失败!";
                    }
                }else{
                    cout << "节点已用完，创建子目录失败！";
                }
            }
        }
    }else {
        cout << "你没权限创建";
    }
    writenode(inode,road[num-1]);
}

/**
 * 删除目录
 * @param dirname
 * @param index
 */
void rmdir(char * dirname, int index) {
    /**
     * 当前目录下删除目录，将删除的目录可能非空，
     * 策略两种：
     * 1. 只删除空目录
     * 2. 递归将非空目录的所有子目录删除，然后再删除自己，第一种实现比较简单，采用第二种
     */
     // TODO t 暂时不清楚意义
     int t = 1;  // 目前不知道是统计什么的，应该其他函数是有用到的
     INODE inode, inode2;
     DIR dir;
    readinode(index, inode);
    if (havePower(inode)) {
        // i:待删除的子目录下标，index2为目录项种的待删子目录的节点
        int i, index2;
        if (havesame(dirname, inode, i, index2)) {
            readinode(index2, inode2);
            if (havePower(inode2)) {
                if (inode2.mode[0] == 'd') {
                    if (inode2.fsize != 0) {
                        char yes = 'y';
                        if (t == 1) {
                            cout << "该目录为非空，删除的话，将失去目录的所有文件，要继续吗？(y/n)";
                            cin >> yes;
                        }
                        if ((yes == 'y') || (yes == 'Y')) {
                            // 遍历待删除子目录（inode2）所有的子目录，递归将其删除
                            char name[14];
                            int index3;
                            INODE inode3;
                            // add  by zhanglianyong

                            for (int i = 0; i < (inode2.fsize / 36); i++) {
                                openDisk();
                                disk.seekg(inode2.addr[0] * 514 + 36 * i);
                                disk >> name;
                                disk >> index3;
                                closeDisk();
                                readinode(index3, inode3);
                                if (inode3.mode[0] == 'd') {
                                    rmdir(name, index2);
                                } else {
                                    rmForDir(name, index2);
                                }
                            }
                            rmdir(dirname, index);
                        } else {
                            cout << "目录删除失败";
                        }
                    } else {
                        // 空目录删除
                        bfree(inode2.addr[0]);
                        ifree(index2);
                        // 对当前目录盘块的修改，inode.addr[0]为当前盘块号，i为待删子目录子目录项下标
                        openDisk();
                        disk.seekp(514 * inode.addr[0] + 36 * i);
                        disk << setw(36) << "";
                        closeDisk();
                        for (int j = i + 1;j < (inode.fsize / 36); j++) {
                            readdir(inode, j, dir);
                            writedir(inode, dir, j-1);
                        }
                        // 对当前目录节点的修改
                        inode.fsize -= 36;
                        const string &ctime = getTime();
                        strcpy(inode.ctime, ctime.c_str());
                    }
                    cout << "目录成功删除";
                    // 修改当前节点
                    inode.fsize -= 36;
                    const string &ctime1 = getTime();
                    strcpy(inode.ctime, ctime1.c_str());
                } else {
                    cout << "数据文件应用rm命令删除";
                }
            } else {
                cout << "你没有权限";
            }
        } else {
            cout << "目录中不存在该子目录";
        }
    } else {
        cout << "你没有权限";
    }
    writenode(inode, index);
}

/**
 * 显示当前节点的所有子目录
 */
void ls() {
    // 显示当前节点的所有子目录
    INODE inode, inode2;
    readinode(road[num - 1], inode);
    char name[14];
    int index;
    // 编译所有的目录项
    for (int i = 0; i < (inode.fsize / 36); ++i) {
        openDisk();
        disk.seekp(inode.addr[0]* 514 + 36 * i);
        disk >> name;
        disk >> index;
        closeDisk();
        readinode(index, inode2);
        cout << setw(15) << name << setw(6) << inode2.fsize << setw(6) << inode2.owner;
        cout << setw(6) << inode2.group << setw(12) << inode2.mode << setw(10) << inode2.ctime << endl;

    }

}

/**
 * 改变目录
 * @param string
 */
void cd(char * string) {
    /*
     * 改变当前目录，有4种处理过程：当string="."时，表示切换当前目录，当string=".."时，表示切换到父目录，
     * 当string="/"，表示切换到根目录；string为一路径时，则调用bool find(char *string)切换到指定目录
     */
    if (!strcmp(string, ".")) {
        cout << "已切换到当前目录";
        return ;
    }
    if (!strcmp(string, "/")) {
        strcmp(curName, "root");
        road[0] = 0;
        num = 1;
        cout << "已切换到根目录";
        return;
    }
    // 父目录
    if (!strcmp(string, "..")) {
        if (strcmp(curName, "root")) {
            INODE inode;
            readinode(road[num - 2], inode);
            char name[14];
            openDisk();
            disk.seekp(inode.addr[0] * 514 + 18);
            disk >> name;
            strcpy(curName, name);
            num--;
            closeDisk();
            cout << "已切换到父目录";
            return;
        }
        cout << "已经根目录";
        return;
    }

    char * per = strchr(string, (int)'/');
    if (per == NULL) {
        INODE inode, inode2;
        int i, index2;
        readinode(road[num - 1], inode);
        char name[14];
        if (havesame(string, inode, i, index2)) {
            readinode(index2, inode2);
            if (inode2.mode[0] == 'd') {
                openDisk();
                disk.seekp(514 * inode.addr[0] + 36 * i);
                disk >> name;
                closeDisk();
                strcpy(curName, name);
                road[num] = index2;
                num++;
                cout << "已经切换到子目录";
                return;
            }
            cout << "不能根据路径找到相关目录，因为" << string << "是数据文件";
        } else {
            cout << "该子目录不存在，不能根据路径找到相关目录";
        }
    } else {
        // 根据指定路径切换目录
        char tempCurName[14];
        int troad[20];
        int tnum = num;
        strcpy(tempCurName, curName);
        for (int i = 0; i < num; ++i) {
            troad[i] = road[i];
        }
        if (find(string)) {
            INODE inode;
            readinode(road[num - 1], inode);
            if (inode.mode[0] == 'd') {
                cout << "已切换到该目录0";
                return;
            }
        }
        cout << "不能根据路径找到相关目录";
        strcpy(curName, tempCurName);
        num = tnum;
        for (int ii = 0; ii < tnum; ++ii) {
            road[ii] = troad[ii];
        }
    }

}

/**
 * 登录
 * @return
 */
bool login() {
    // 登录信息，要求输入用户信息，并判断是否合法
    char auser2[6];
    char apwd2[6];
    cout << "you name?";
    cin >> curUserName;
    cout << "you password?";
    cin >> curPassword;
    bool have = false;
    openUser();
    for (int n = 0; n < USER_NUM; ++n) {
        // username, password, group 长度6
        user.seekp(18 * n);
        user >> auser2 >> apwd2;
        int a = strcmp(curUserName, auser2);
        int b = strcmp(curPassword, apwd2);
        if (!a && !b) {
            have = true;
            user >> curgroup;
            break;
        }
    }
    closeUser();
    return have;
}

/**
 * 修改密码
 */
void changePassword() {
    char aUser2[6];
    char aPwd2[6];
    cout << "请输入原有密码：";
    cin >> aPwd2;
    if (!strcmp(curPassword, aPwd2)) {
        openUser();
        int n;
        for (n = 0; n < USER_NUM; n++) {
            user.seekp(18 * n);
            user >> aUser2;
            if (!strcmp(curUserName, aUser2)) {
                user >> curgroup;
                break;
            }
        }
        cout << "请输入密码：" << endl;
        cin >> aPwd2;
        user.seekp(18 * n + 6);
        user << setw(6) << aPwd2;
        cout << "密码修改成功";
        closeUser();
    } else {
        cout << "输入错误";
    }
}

/**
 * 判断写权限
 * @param inode
 * @return
 */
bool havePower(INODE inode) {
    // 判断当前用户对指定的节点有无写权限
    if (!strcmp(curUserName, inode.owner)) {
        if (inode.mode[2] == 'w') {
            return true;
        } else {
            return false;
        }
    } else {
        // 在当前用户组
        if (!strcmp(curgroup, inode.owner)) {
            if (inode.mode[5] == 'w') {
                return true;
            } else {
                return false;
            }
        } else {
            // 其他用户
            if (inode.mode[8] == 'w') {
                return true;
            } else {
                return false;
            }
        }
    }
}

// TODO 修改但是文件的权限并没有修改
void chmod(char * name){
    INODE inode,inode2;
    readinode(road[num-1],inode);//
    int i , index2;

    if(havesame(name,inode,i, index2)){
        readinode(index2,inode2);
        if(havePower(inode2)){
            char amode[3];
            cout << "1 表示所有者，4表示组内，7表示其他用户，a表示-wx模式，b表示r-x模式，c表示rwx模式/n";
            cout << "请输入修改方案（例如 4c）:";
            cin >> amode;
            if(amode[0]='1' || amode[0]=='4' || amode[0]=='7'){
                if(amode[1]=='a'){
                    inode2.mode[(int)amode[0]-48]='-';
                    inode2.mode[(int)amode[0]+1-48]='w';
                    writenode(inode2,index2);
                    cout << "修改完毕";
                }else{
                    if(amode[1]=='b'){
                        inode2.mode[(int)amode[0]-48]='r';
                        inode2.mode[(int)amode[0]+1-48]='-';
                        writenode(inode2,index2);
                        cout<<"修改完毕";
                    }else{
                        if(amode[1]=='c'){
                            inode2.mode[(int)amode[0]-48]='r';
                            inode2.mode[(int)amode[0]+1-48]='w';
                            writenode(inode2,index2);
                        }else{
                            cout << "输入不合法";
                            return ;
                        }
                    }
                }
            }else{
                cout<<"输入不合法!!!";
                return;
            }
        }else{
            cout<<"你无权修改该子目录或文件";
            return ;
        }
    }else{
        cout<<"不存在该子目录或文件";
        return ;
    }

    const string &ctime = getTime();
    strcpy(inode.ctime,ctime.c_str());
    strcpy(inode2.ctime,ctime.c_str());
    writenode(inode,road[num-1]);
    writenode(inode2,index2);
    return;
}


void chown(char *name){
    INODE inode,inode2;
    readinode(road[num-1],inode);
    int i,index2;
    if( havePower(inode)){
        if( havesame(name,inode,i,index2) ){
            readinode(index2,inode2);
            if( havePower(inode2) ){
                char owner2[6];
                char auser2[6];
                char group2[6];
                cout<<"请输入改后的文件所有者:";
                cin>>owner2;
                bool is=false;
                user.open("user.txt",ios::in);
                for( int n=0;n<USER_NUM;n++ ){
                    user.seekg(18*n);
                    user>>auser2;
                    if( !strcmp(owner2,auser2)){
                        is=true;
                        user.seekg(18*n+12);
                        user>>group2;
                        break;
                    }
                }

                user.close();
                if(is){
                    strcpy(inode2.owner,owner2);
                    strcpy(inode2.group,group2);
                    writenode(inode2,index2);
                    cout<<"改成";
                    const string &ctime = getTime();
                    strcpy(inode.ctime,ctime.c_str());
                    strcpy(inode2.ctime,ctime.c_str());
                    writenode(inode,road[num-1]);
                    writenode(inode2,index2);
                }else{
                    cout << "不存在该用户，修改失败";
                }
            }else{
                cout << "你没有权限";
            }
        }else{
            cout << "不存在该子目录或文件";
        }
    }else{
        cout << "你没有权限";
    }
}

/**
  改变文件的所属组
  **/
void chgrp(char *name){/*改变文件所属组*/
    INODE inode, inode2;
    readinode(road[num-1], inode);
    int i,index2;
/*当前节点写入节点对象*/
/*i为目录项下标，index2为目录项中节点号*/
    if( havePower(inode)){
        if( havesame(name,inode,i,index2)){
            readinode(index2,inode2);
            if(havePower(inode2)){
                char group2[6];
                char agroup2[6];
                cout<<"请输入改后的文件所属组:";
                cin>>group2;
                bool is=false;/*判断输入的也是合法用户名*/
                openUser();
                for( int n=0;n<USER_NUM;n++){
                    /*用户名18n+0~18n+5，密码18n+6~18n+11 用户组18n+12~18n+17*/
                    user.seekg(18*n+12);
                    user>>agroup2;
                    if(!strcmp(group2,agroup2)){
                        is=true;
                        break;
                    }
                }
                closeUser();
                if (is){
                    strcpy(inode2.group,group2);
                    writenode(inode2,index2);
                    cout<<"修改成功";
                    /*修改当前节点和子节点*/
                    const string &ctime = getTime();
                    strcpy(inode.ctime,ctime.c_str());
                    strcpy(inode2.ctime,ctime.c_str());
                    writenode(inode,road[num-1]);
                    writenode(inode2,index2);
                }else{
                    cout<<"不存在该组，修改失败";
                }
            }else{
                cout<<"你没有权限";
            }
        }else{
            cout<<"不存在该子目录或文件";
        }
    }else{
        cout<<"你没有权限";
    }
}

/**
 * 改变文件名
 * **/
void chnam(char * name) {
    INODE inode,inode2;
    readinode(road[num-1],inode);/*当前节点写入节点对象*/
    int i,index2;/*i为目录项下标，index2为目录项中节点号*/
    if( havePower(inode)) {
        if( havesame(name,inode,i,index2)){
            readinode(index2,inode2);
            if( havePower(inode2)) {
                char name2[14];
                cout<<"请输入更改后的文件名:";
                cin>>name2;
                openDisk();
                disk.seekp( 514*inode.addr[0]+36*i);
                disk<<setw(15)<<name2;
                disk.close();
                /*如是录文件，其下的所有目录项都要改parame[14]*/
                if(inode2.mode[0]='d'){
                    openDisk();
                    for(int i=0;i<(inode2.fsize/36);i++)
                        {
                            disk.seekg(514*inode2.addr[0]+36*i+18);
                            disk<<setw(15)<<name2;
                        }
                    disk.close();
                }
                cout<<"更改成功";
                /*修改当前节点和子节点*/
                const string &ctime = getTime();
                strcpy(inode.ctime, ctime.c_str());
                strcpy(inode2.ctime, ctime.c_str());
                strcpy(inode2.ctime, ctime.c_str());
                writenode(inode,road[num-1]);
                writenode(inode2,index2);
            }else{
                cout<<"你没有权限";
            }
        }else{
            cout<<"不存在该子录或文件";
        }
    }else{
        cout<<"你没有权限";
    }
}

/**
 * 命令行解析函数
 */
void getCommand() {
    char command[10];
    bool have;
    for (;;) {
        have = false;
        cout << "\n";
        printRoad();
        cout << ">";
        cin >> command;
        // 改变当前目录，若为文件名，则切换到子目录；若为”.."，则切换到父目录；若为“/”则切换到根目录
        if (!strcmp(command, "cd")) {
            have = true;
            // 路径，至少有一个"/"以root开头，不以“/”结尾
            char string[100];
            cin >> string;
            getLog(command, string);
            cd(string);
        }
        if (!strcmp(command, "mksome")) {
            // 构建基本文件结构
            have = true;
            getLog(command, "");
            cout << "bin";
            mkdir("bin");
            cout << "\ndev";
            mkdir("dev");
            cout << "\nlib";
            mkdir("lib");
            cout << "\netc";
            mkdir("etc");
            cout <<"\nusr";
            mkdir("usr");
            cout << "\ntmp";
            mkdir("tmp");
        }
        // 在当前目录下创建目录
        if (!strcmp(command, "mkdir")) {
            have = true;
            char dirname[14];
            cin >> dirname;
            getLog(command, dirname);
            mkdir(dirname);
        }
        // 在当前目录下删除目录，1只能删除空目录 2. 提醒删除所有子目录
        if (!strcmp(command, "rmdir")) {
            have = true;
            char dirname[14];
            cin >> dirname;
            getLog(command, dirname);
            rmdir(dirname, road[num - 1]);
        }
        // 在当前目录下创建数据文件
        if (!strcmp(command, "mk")) {
            have = true;
            char filename[14];
            cin >> filename;
            getLog(command, filename);
            cout << "请先输入文件内容（1-2048）";
            char content[2048];
            cin >> content;
            mk(filename, content);
        }

        // 把指定目录下的指定文件负复制到当前目录下
        if (!strcmp(command, "cp")) {
            have = true;
            char string[100];
            cin >> string;
            getLog(command, string);
            cp(string);
        }

        // 删除当前目录下的数据文件
        if (!strcmp(command, "rm")) {
            have = true;
            char filename[14];
            cin >> filename;
            getLog(command, filename);
            rm(filename);
        }
        if (!strcmp(command, "cat")) {
            have = true;
            char filename[14];
            cin >> filename;
            getLog(command, filename);
            cat(filename);
        }
        // 改变文件权限
        if (!strcmp(command, "chmod")) {
            have = true;
            char name[14];
            cin >> name;
            getLog(command, name);
            chmod(name);
        }
        // 改变文件所有者（如所有者在另一个组，那么组也要改变
        if (!strcmp(command, "chown")) {
            have = true;
            char name[14];
            cin >> name;
            getLog(command, name);
            chown(name);
        }
        // 改变文件所属组
        if (!strcmp(command, "chgrp")) {
            have = true;
            char name[14];
            cin >> name;
            getLog(command, name);
            chgrp(name);
        }
        // 改变文件名
        if (!strcmp(command, "chnam")) {
            have = true;
            char name[14];
            cin >> name;
            getLog(command, name);
            chnam(name);
        }
        // 对当前目录的操作
        if (!strcmp(command, "pwd")) {
            getLog(command, "");
            have = true;
            cout << "您的当前目录为：" << curName;
        }
        // 显示所有的子目录
        if (!strcmp(command, "ls")) {
            have = true;
            getLog(command, "");
            ls();
        }
        // 用户组的操作
        if (!strcmp(command, "login")) {
            have = true;
            cout << "账号：" << curUserName << "已注销\n";
            getLog(command, "");
            while (!login()) {
                cout << "密码错误!!!\n";
            }
            cout << "登录成功！！！";
            cout << "欢迎" << curUserName ;
        }
        // 改变用户口令
        if (!strcmp(command, "passwd")) {
            have = true;
            getLog(command, "");
            changePassword();
        }
        // 系统重置
        if (!strcmp(command, "reset")) {
            have = true;
            getLog(command, "");
            initial();
            cout << "系统已重置";
        }
        if (!strcmp(command, "exit")) {
            getLog(command, "");
            have = true;
            writesuper();
            return;
        }
        if (!have) {
            cout << command << "不是合法的命令"<< endl;
        }
    }
}

void printRoad() {
    // 根据节点路径，输出路径
    cout << "root";
    INODE inode;
    int nextIndex;
    char name[14];
    for (int i = 0; i+1 < num; ++i) {
        readinode(road[i], inode);
        openDisk();
        for (int j = 0; j < (inode.fsize / 36); ++j) {
            // 编译所有的目录项
            disk.seekp(514 * inode.addr[0] + 36 *j);
            disk >> name;
            disk >> nextIndex;
            if (nextIndex == road[i+1]) {
                cout << "/";
                cout << name;
                break;
            }
        }
        closeDisk();
    }
}
