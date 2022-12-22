//
// Created by 大勇 on 2022/12/9.
//

#ifndef FILESYSTEM_CONFIG_H
#define FILESYSTEM_CONFIG_H

#endif //FILESYSTEM_CONFIG_H

/**
 * 超级块
 */
struct SUPER_BLOCK {
    /**
     * 空闲节点号栈 setw(3) * 80
     */
    int fistack[80];
    /**
     * 空闲节点栈指针
     */
    int fiptr;
    /**
     * 空闲盘块号栈
     */
    int fbstack[10];
    /**
     * 空闲盘块栈指针
     */
    int fbptr;
    /**
     * 空闲节点总数
     */
    int inum;
    /**
     * 空闲盘总数
     */
    int bnum;
};
/**
 * 保证两个数据之间由空格隔开
 */
struct INODE {
    /**
     * 文件大小
     */
    int fsize;
    /**
     * 文件盘块数
     */
    int fbnum;
    /**
     * 4个直接盘块号
     */
    int addr[4];
    /**
     * 一个一次间址
     */
    int addr1;
    /**
     * 一个两次间址
     */
    int addr2;
    /**
     * 文件所有者
     */
    char owner[6];
    /**
     * 文件所属组
     */
    char group[6];
    /**
     * 文件类别及存储权限
     */
    char mode[11];
    /**
     * 最近修改时间
     */
    char ctime[9];
};

/**
 * 目录项
 */
struct DIR {
    /**
     * 当前目录文件名
     */
    char fname[14];
    /**
     * 节点名
     */
    int index;
    /**
     * 父目录名
     */
    char parfname[14];
    /**
     * 父目录节点号
     */
    int parindex;
};
// TODO 这个在login中使用，目前未知定义
#define USER_NUM 3
/**
 * 配置一个变量维护全局的用户和用户组
 */
char curUserName[6];
char curgroup[6];
char curPassword[6];

char diskFile[100] = "D:\\clion_code\\file_system\\disk.txt";
char operatorLogFile[100] = "D:\\clion_code\\file_system\\operation.log";
char controlFile[100] = "D:\\clion_code\\file_system\\control.txt";
char userFile[100] = "D:\\clion_code\\file_system\\user.txt";