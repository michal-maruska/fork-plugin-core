#ifndef FORK_ENUMS_H
#define FORK_ENUMS_H

/** Sub-OP-codes for the Set/Get-Configure-key request:  **/
enum  {
       fork_configure_last_events,
       fork_configure_overlap_limit,
       fork_configure_total_limit,
       fork_configure_repeat_limit,

       fork_configure_repeat_consider_forks,
       /* only per key: */
       fork_configure_key_fork,
       fork_configure_key_fork_repeat, // 7

       fork_configure_debug,
       fork_configure_switch,
       fork_configure_clone,
       fork_configure_clear_interval,

       fork_server_dump_keys, // 11
       fork_client_dump_keys,
};


enum {
  fork_GLOBAL_OPTION = 0,
  fork_LOCAL_OPTION = 1,
  fork_TWIN_OPTION = 2,
};

#endif // FORK_ENUMS_H
