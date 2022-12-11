//
// Created by 大勇 on 2022/12/9.
//

#ifndef FILE_SYSTEM_FUNCTIONDEFINE_H
#define FILE_SYSTEM_FUNCTIONDEFINE_H

#endif //FILE_SYSTEM_FUNCTIONDEFINE_H

int ialloc(void);
void ifree(int index);
void readinode(int index, INODE & inode);
void initial();
void ifree(int index);
void readinode(int index, INODE & inode);
void writenode(INODE inode, int index);
int balloc();
void bfree(int index);
void readsuper();
void writesuper();
void readdir(INODE inode, int index, DIR & dir);
void writedir(INODE inode, DIR dir, int index);
void mk(char * filename, char * content);
void rm(char * filename);
void cp(char * string);
void cat(char * filename);
bool havesame(char *dirname, INODE inode, int & i, int &index2);
bool find(char * string);
void mkdir(char * dirname);
void rmdir(char * dirname, int index);
void ls();
void cd(char * string);
bool login();
void changePassword();
bool havePower(INODE inode);
void chmod(char * name);
void chown(char * name);
void chgrp(char * name);
void chnam(char * name);
void getCommand();
void printRoad();
