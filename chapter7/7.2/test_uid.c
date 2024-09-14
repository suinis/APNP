#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

//将以root身份启动的进程转换为以一个普通用户身份运行
static bool switch_to_user(__uid_t user_id, __gid_t gp_id){
    //确保目标用户不是root
    if(user_id == 0 && gp_id == 0) return false;

    //确保当前用户是合法用户：root或者目标用户
    __gid_t gid = getgid();
    __uid_t uid = getuid();
    if((gid == 0 || uid == 0) && (gid != gp_id || uid != user_id)) return false;

    //如果不是root，则已经是目标用户
    if(uid != 0) return true;

    //切换到目标用户
    if((setgid(gp_id) < 0) || (setuid(user_id) < 0)) return false;
    return true; 
}

int main(){
    __uid_t uid = getuid();
    __uid_t euid = geteuid();
    __gid_t gid = getgid();
    __gid_t egid = getegid();
    printf("userid is : %d, effective userid is : %d, groupid is : %d, effective groupid is : %d\n", uid, euid, gid, egid);

    if(setuid(0) < 0) printf("setuid(0) failed!\n");
    else printf("setuid(0) successed!\n");
    printf("userid is : %d, effective userid is : %d, groupid is : %d, effective groupid is : %d\n", getuid(), geteuid(), getgid(), getegid());

    bool isSwitch = switch_to_user(1001, 1001);
    printf("Switch result : %d, userid is : %d, effective userid is : %d, groupid is : %d, effective groupid is : %d\n",isSwitch, getuid(), geteuid(), getgid(), getegid());
    return 0;
}