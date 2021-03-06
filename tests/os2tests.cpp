//
// Created by Miki Mints on 12/5/18.
//
#include "macros.h"
#include "hw2_syscalls.h"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sched.h>
#include <list>
#include <algorithm>

bool testChange() {
    // Process is not SC
    ASSERT_FALSE(is_changeable(getpid()));
    ASSERT_EQUALS(get_policy(getpid()), -1);
    ASSERT_EQUALS(errno, EINVAL);

    for (int i = 0; i < 100; ++i) {
        if (CHILD_PROCESS(fork())) {
            CHILD_ASSERT_EQUALS(get_policy(getpid()), -1);
            CHILD_EXIT();
        }
    }

    int first_pid = fork();
    if(CHILD_PROCESS(first_pid)) {
        CHILD_ASSERT_EQUALS(make_changeable(getpid()), 0);
        change(1);
        CHILD_ASSERT_EQUALS(get_policy(getpid()), 1);
        for (int i = 0; i < 100; ++i) {
            if(CHILD_PROCESS(fork())) {
                CHILD_ASSERT_EQUALS(get_policy(getpid()), 1);
                CHILD_EXIT();
            }
        }
        int status = 0;
        while (wait(&status) > 0) { if(status) exit(status); }
        CHILD_EXIT();
    } else {
        int status = 0;
        WAIT_CHILD_PID(status, first_pid);

        // Policy should be turned off
        // TODO Fix
        ASSERT_EQUALS(make_changeable(getpid()), 0);
        ASSERT_FALSE(get_policy(getpid()));
        return true;
    }
}

/**
 * This test forks 2 processes - pid and pid2, in order to test make_changeable restriction on different policy
 * processes.
 * pid2 - worker process which changes the shceduling regime of other processes
 * pid - busy-waiting process which is kept alive until all assertions are complete
 */
bool testMakeChangeable() {
    int ppid = getpid();
    ASSERT_FALSE(is_changeable(ppid));
    ASSERT_EQUALS(get_policy(ppid), -1);
    int pid2 = fork();
    if(CHILD_PROCESS(pid2)) {
        // Second child
        // Keep this child alive until both Father and First Child finish
        int code = 0;
        waitpid(ppid, NULL, 0);
        CHILD_EXIT();
    } else {
        int pid = fork();
        if(CHILD_PROCESS(pid)) {
            // First child

            // Before First Child finishes, change the Second Child to RR (Real time)
            struct sched_param param;
            param.sched_priority = 99;
            CHILD_ASSERT_EQUALS(sched_setscheduler(pid2, SCHED_RR, &param), 0);

            CHILD_EXIT();
        } else {
            // Father process
            int code = 0;
            WAIT_CHILD_PID(code, pid);
            // First Child finished
            // Father == SO, Second Child == RR
            ASSERT_EQUALS(make_changeable(pid2), -1);
            ASSERT_EQUALS(errno, EINVAL); // Can't change Real Time processes

            ASSERT_FALSE(is_changeable(getpid()));
            ASSERT_EQUALS(make_changeable(getpid()), 0);
            ASSERT_TRUE(is_changeable(getpid()));
            ASSERT_EQUALS(make_changeable(pid2), -1);

            ASSERT_EQUALS(make_changeable(-1), -1);
            ASSERT_EQUALS(errno, ESRCH);
            ASSERT_EQUALS(make_changeable(65000), -1);
            ASSERT_EQUALS(errno, ESRCH);
        }
    }
    return true;
}

bool testIsChangeable() {
    ASSERT_FALSE(is_changeable(getpid()));

    int pid = fork();
    if(CHILD_PROCESS(pid)) {
        int status = is_changeable(getpid());
        CHILD_ASSERT_FALSE(status);
        change(1);
        // is_changeable should only test for process' policy
        CHILD_ASSERT_FALSE(is_changeable(getpid()));
        CHILD_EXIT();
    } else {
        int code = 0;
        WAIT_CHILD(code);
    }

    ASSERT_EQUALS(make_changeable(getpid()), 0);
    ASSERT_TRUE(is_changeable(getpid()));

    pid = fork();
    if(CHILD_PROCESS(pid)) {
        CHILD_ASSERT_TRUE(is_changeable(getpid()));
        CHILD_EXIT();
    } else {
        int code = 0;
        WAIT_CHILD(code);
        // Child process does not exist anymore
        ASSERT_EQUALS(is_changeable(pid), -1);
        ASSERT_EQUALS(errno, ESRCH);
    }

    ASSERT_EQUALS(is_changeable(-1), -1);
    ASSERT_EQUALS(errno, ESRCH);

    return true;
}

bool testChangeAndGetPolicy() {
    // Father process is still CHANGEABLE
    ASSERT_TRUE(is_changeable(getpid()));

    ASSERT_EQUALS(change(-1), -1);
    ASSERT_EQUALS(errno, EINVAL);

    ASSERT_EQUALS(change(65000), -1);
    ASSERT_EQUALS(errno, EINVAL);

    // No regime change here
    ASSERT_EQUALS(change(0), 0);
    ASSERT_EQUALS(change(1), 0);

    int ppid = getpid();
    ASSERT_TRUE(get_policy(getpid()));
    ASSERT_EQUALS(get_policy(getppid()), -1); // Parent is not SC
    ASSERT_EQUALS(errno, EINVAL);
    for (int i = 0; i < 50; ++i) {
        int p = fork();
        if (CHILD_PROCESS(p)) {
            // All child processes busy-wait for father to finish
            CHILD_ASSERT_TRUE(get_policy(getpid()));
            waitpid(ppid, NULL, 0);
            CHILD_EXIT();
        }
    }

    ASSERT_EQUALS(get_policy(-1), -1);
    ASSERT_EQUALS(errno, ESRCH);

    return true;
}

int main() {
    int status = 0;
    RUN_TEST_ISOLATE(testChange, status);
    RUN_TEST_ISOLATE(testMakeChangeable, status);
    RUN_TEST(testIsChangeable);
    RUN_TEST(testChangeAndGetPolicy);

    if(status == 0) {
        cout << GREEN << "[SUCCESS] Functional tests passed!" << NORMAL_TEXT << endl;
    }
    return 0;

}
