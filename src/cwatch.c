/* cwatch.c
 * Monitor file system activity using the inotify linux kernel library
 *
 * Copyright (C) 2012, Giuseppe Leone <joebew42@gmail.com>,
 *                     Vincenzo Di Cicco <enzodicicco@gmail.com>
 *
 * This file is part of cwatch
 *
 * cwatch is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cwatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "cwatch.h"

/* Command line long options */
static struct option long_options[] =
{
    /* Options that set flags */
    
    /*
     {"verbose",       no_argument, &verbose_flag, TRUE},
     {"log",           no_argument, &syslog_flag,  TRUE},
    */
    
    /* Options that set index */
    {"command",       required_argument, 0, 'c'}, /* exclude format */
    {"format",        required_argument, 0, 'F'}, /* exclude command */
    {"directory",     required_argument, 0, 'd'},
    {"events",        required_argument, 0, 'e'},
    {"exclude",       required_argument, 0, 'x'},
    {"no-symlink",    no_argument,       0, 'n'},
    {"recursive",     no_argument,       0, 'r'},
    {"verbose",       no_argument,       0, 'v'},
    {"syslog",        no_argument,       0, 'l'},
    {"version",       no_argument,       0, 'V'},
    {"help",          no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

/*
 * The inotify events LUT
 * for a complete reference of all events, look here:
 * http://tomoyo.sourceforge.jp/cgi-bin/lxr/source/include/uapi/linux/inotify.h
 */
static struct event_t events_lut[] =
{
    {"access",        event_handler_undefined},  /* IN_ACCESS */
    {"modify",        event_handler_undefined},  /* IN_MODIFY */
    {"attrib",        event_handler_undefined},  /* IN_ATTRIB */
    {"close_write",   event_handler_undefined},  /* IN_CLOSE_WRITE */
    {"close_nowrite", event_handler_undefined},  /* IN_CLOSE_NOWRITE */
    {"open",          event_handler_undefined},  /* IN_OPEN */
    {"moved_from",    event_handler_moved_from}, /* IN_MOVED_FROM */
    {"moved_to",      event_handler_moved_to},   /* IN_MOVED_TO */
    {"create",        event_handler_create},     /* IN_CREATE */
    {"delete",        event_handler_delete},     /* IN_DELETE */
    {"delete_self",   event_handler_undefined},  /* IN_DELETE_SELF */
    {"move_self",     event_handler_undefined},  /* IN_MOVE_SELF */
    {NULL,            event_handler_undefined},
    {"umount",        event_handler_undefined},  /* IN_UMOUNT */
    {"q_overflow",    event_handler_undefined},  /* IN_Q_OVERFLOW */
    {"ignored",       event_handler_undefined},  /* IN_IGNORED */
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {"onlydir",       event_handler_undefined},  /* IN_ONLYDIR */
    {"dont_follow",   event_handler_undefined},  /* IN_DONT_FOLLOW */
    {"excl_unlink",   event_handler_undefined},  /* IN_EXCL_UNLINK */
    {NULL,            event_handler_undefined},
    {NULL,            event_handler_undefined},
    {"mask_add",      event_handler_undefined},  /* IN_MASK_ADD */
    {"isdir",         event_handler_undefined},  /* IN_ISDIR */
    {"oneshot",       event_handler_undefined},  /* IN_ONESHOT */
    
    /* threated as edge cases (see get_inotify_event implementation) */
    {"close",         event_handler_undefined},  /* 32. IN_CLOSE */
    {"move",          event_handler_undefined},  /* 33. IN_MOVE */
    {"all_events",    event_handler_undefined},  /* 34. IN_ALL_EVENTS */
    
};

void print_version()
{
    printf("%s %s (%s)\n"
           "License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>\n"
           "This is free software: you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n",
           PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_STAGE);
}

int help(int error)
{
    printf("Usage: %s -c COMMAND -d DIRECTORY [-v] [-s] [-options]\n", PROGRAM_NAME);
    printf("   or: %s -F FORMAT  -d DIRECTORY [-v] [-s] [-options]\n", PROGRAM_NAME);
    printf("   or: %s [-V|--version]\n", PROGRAM_NAME);
    printf("   or: %s [-h|--help]\n\n", PROGRAM_NAME);
    printf("  -c --command COMMAND\n");
    printf("     Execute a user-specified command.\n");
    printf("     Injection of specal special characters is possible\n");
    printf("     (See the TABLE OF SPECIAL CHARACTERS)\n");
    printf("     warn: This option exclude the use of -F option\n\n");
    printf("  -F --format  FORMAT\n");
    printf("     Output in a user-specified format, using printf-like syntax.\n");
    printf("     This usage is useful if you want to emebed %s inside a bash script.\n", PROGRAM_NAME);
    printf("     Injection of specal special characters is possible\n");
    printf("     (See the TABLE OF SPECIAL CHARACTERS)\n");
    printf("     warn: This option exclude the use of -c and -v option\n\n");
    printf("  *TABLE OF SPECIAL CHARACTERS*\n\n");
    printf("       %sr : full path of the root DIRECTORY\n", "%");
    printf("       %sp : full path of the file/directory where the event occurs\n", "%");
    printf("       %sf : full path of the file/directory that triggered the event\n", "%");
    printf("       %se : the type of the occured event (the the list below)\n\n", "%");
    printf("  -d  --directory DIRECTORY\n");
    printf("      The directory to monitor\n\n");
    printf("  *LIST OF OTHER OPTIONS*\n\n");
    printf("  -e  --events [event,[event,[,..]]]\n");
    printf("      Specify which type of events to monitor. List of events:\n");
    printf("        access           : File was modified\n");
    printf("        modify           : File was modified\n");
    printf("        attrib           : File attributes changed\n");
    printf("        close_write      : File closed, after being opened in writeable mode\n");
    printf("        close_nowrite    : File closed, after being opened in read-only mode\n");
    printf("        close            : File closed, regardless of read/write mode\n");
    printf("        open             : File was opened\n");
    printf("        moved_from       : File was moved out of watched directory.\n");
    printf("        moved_to         : File was moved into watched directory.\n");
    printf("        move             : A file/dir within watched directory was moved\n");
    printf("        create           : A file was created within watched directory\n");
    printf("        delete           : A file was deleted within watched directory\n");
    printf("        delete_self      : The watched file was deleted\n");
    printf("        unmount          : File system on which watched file exists was unmounted\n");
    printf("        q_overflow       : Event queued overflowed\n");
    printf("        ignored          : File was ignored\n");
    printf("        isdir            : Event occurred against dir\n");
    printf("        oneshot          : Only send event once\n");
    printf("        all_events       : All events\n");
    printf("        default          : modify, create, delete, move.\n\n");
    printf("  -n  --no-symlink\n");
    printf("      Do not traverse symbolic link\n\n");
    printf("  -r  --recursive\n");
    printf("      Enable the recursively monitor of the directory\n\n");
    printf("  -x  --exclude <regex>\n");
    printf("      Do not process any events whose filename matches the specified POSIX\n");
    printf("      POSIX extended regular expression, case sensitive \n\n");
    printf("  -v  --verbose\n");
    printf("      Verbose mode\n\n");
    printf("  -s  --syslog\n");
    printf("      Verbose mode through syslog\n\n");
    printf("  -h  --help\n");
    printf("      Print this help and exit\n\n");
    printf("  -V  --version\n");
    printf("      Print the version of the program and exit\n\n");
           
    printf("Reports bugs to: <https://github.com/joebew42/cwatch/issues/>\n");
    printf("%s home page: <https://github.com/joebew42/cwatch/>\n", PROGRAM_NAME);
    
    if (error)
        exit(error);

    return 0;
}

void log_message(char *message)
{
    if (verbose_flag && (NULL == format))
        printf("%s\n", message);
    
    if (syslog_flag) {
        openlog(PROGRAM_NAME, LOG_PID, LOG_LOCAL1);
        syslog(LOG_INFO, message);
        closelog();
    }
    
    free(message);
}

char *resolve_real_path(const char *path)
{
    char *resolved = malloc(MAXPATHLEN + 1);
    
    realpath(path, resolved);
    
    if (resolved == NULL)
        return NULL;
    
    strcat(resolved, "/");
     
    return resolved;
}

LIST_NODE *get_from_path(const char *path)
{
    LIST_NODE *node = list_wd->first;
    while (node) {
        WD_DATA *wd_data = (WD_DATA *) node->data;
        if (strcmp(path, wd_data->path) == 0)
            return node;
        node = node->next;
    }
    
    return NULL;
}

LIST_NODE *get_from_wd(const int wd)
{
    LIST_NODE *node = list_wd->first;
    while (node) {
        WD_DATA *wd_data = (WD_DATA *) node->data;
        if (wd == wd_data->wd)
            return node;
        node = node->next;    
    }
    
    return NULL;
}

int parse_command_line(int argc, char *argv[])
{
    if (argc == 1) {
        help(1);
    }
    
    /* Handle command line options */
    int c;
    while ((c = getopt_long(argc, argv, "svnrVhe:c:F:d:x:", long_options, NULL)) != -1) {
        switch (c) {
        case 'c': /* --command */
            if (NULL != format)
                help(1);
            
            if (optarg == NULL
                || strcmp(optarg, "") == 0
                || (command = bfromcstr(optarg)) == NULL)
            {
                help(1);
            }
            
            /* Remove both left/right whitespaces */
            btrimws(command);
            
            /* The command will be executed in inline mode */
            execute_command = execute_command_inline;
            
            break;
            
        case 'F': /* --format */
            if (NULL != command)
                help(1);

            format = bfromcstr(optarg);

            /* The command will be executed in embedded mode */
            execute_command = execute_command_embedded;
            
            break;
            
        case 'd': /* --directory */
            if (NULL == optarg || strcmp(optarg, "") == 0)
                help(1);
            
            /* Check if the path has the ending slash */
            if (optarg[strlen(optarg)-1] != '/') {
                root_path = (char *) malloc(strlen(optarg) + 2);
                strcpy(root_path, optarg);
                strcat(root_path, "/");
            } else {
                root_path = (char *) malloc(strlen(optarg) + 1);
                strcpy(root_path, optarg);
            }
            
            /* Check if it is a valid directory */
            DIR *dir = opendir(root_path);
            if (dir == NULL) {
                help(1);
            }
            closedir(dir);
            
            /* Check if the path is absolute or not */
            /* TODO Dealloc after keyboard Ctrl+C interrupt */
            if (root_path[0] != '/') {
                char *real_path = resolve_real_path(root_path);
                free(root_path);
                root_path = real_path;
            }
            
            break;
        
        case 'e': /* --events */
            /* Set inotify events mask */
            split_event = bsplit(bfromcstr(optarg), ',');
            
            if (split_event != NULL) {
                int i;
                for (i = 0; i < split_event->qty; ++i) {
                    if (bstrcmp(split_event->entry[i], bfromcstr("access")) == 0) {
                        event_mask |= IN_ACCESS;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("modify")) == 0) {
                        event_mask |= IN_MODIFY;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("attrib")) == 0) {
                        event_mask |= IN_ATTRIB;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("close_write")) == 0) {
                        event_mask |= IN_CLOSE_WRITE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("close_nowrite")) == 0) {
                        event_mask |= IN_CLOSE_NOWRITE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("close")) == 0) {
                        event_mask |= IN_CLOSE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("open")) == 0) {
                        event_mask |= IN_OPEN;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("moved_from")) == 0) {
                        event_mask |= IN_MOVED_FROM;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("moved_to")) == 0) {
                        event_mask |= IN_MOVED_TO;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("move")) == 0) {
                        event_mask |= IN_MOVE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("create")) == 0) {
                        event_mask |= IN_CREATE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("delete")) == 0) {
                        event_mask |= IN_DELETE;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("delete_self")) == 0) {
                        event_mask |= IN_DELETE_SELF;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("unmount")) == 0) {
                        event_mask |= IN_UNMOUNT;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("q_overflow")) == 0) {
                        event_mask |= IN_Q_OVERFLOW;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("ignored")) == 0) {
                        event_mask |= IN_IGNORED;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("isdir")) == 0) {
                        event_mask |= IN_ISDIR;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("oneshot")) == 0) {
                        event_mask |= IN_ONESHOT;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("all_events")) == 0) {
                        event_mask |= IN_ALL_EVENTS;
                    } else if (bstrcmp(split_event->entry[i], bfromcstr("default")) == 0) {
                        event_mask |= IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE;
                    } else {
                        help(0);
                        printf("\nUnrecognized event or malformed list of events! Please see the help.\n");
                        exit(1);
                    }    
                }

                bstrListDestroy(split_event);
            }
            break;

        case 'x': /* --exclude */
            if (optarg == NULL)
                help(1);
            
            exclude_regex = (regex_t *) malloc(sizeof(regex_t));
            
            if (regcomp(exclude_regex, optarg, REG_EXTENDED | REG_NOSUB) != 0) {
                free(exclude_regex);
                help(0);
                printf("\nThe specified regular expression is not valid.\n");
                exit(1);
            }
            
            break;
            
        case 'v': /* --verbose */
            verbose_flag = TRUE;
            break;
            
        case 'n': /* --no-symlink */
            nosymlink_flag = TRUE;
            break;
            
        case 'r': /* --recursive */
            recursive_flag = TRUE;
            break;
            
        case 's': /* --syslog */
            syslog_flag = TRUE;
            break;
            
        case 'V': /* --version */
            print_version();
            exit(0);
            
        case 'h': /* --help */
                
        default:
            help(0);
            exit(0);
        }
    }
    
    if (root_path == NULL
        || command == format)
    {
        help(1);
    }

    if (event_mask == 0) {
        event_mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE;
    }
    
    return 0;
}

int watch(char *real_path, char *symlink)
{
    /* Add initial path to the watch list */
    LIST_NODE *node = add_to_watch_list(real_path, symlink);
    if (node == NULL)
        return -1;
    
    /* Temporary list to perform a BFS directory traversing */
    LIST *list = list_init();
    list_push(list, (void *) real_path);
    
    DIR *dir_stream;
    struct dirent *dir;
    
    while (list->first != NULL) {
        /* Directory to watch */
        char *p = (char*) list_pop(list);
        
        dir_stream = opendir(p);
        
        if (dir_stream == NULL) {
            printf("UNABLE TO OPEN DIRECTORY:\t\"%s\" -> %d\n", p, errno);
            exit(1);
        }
        
        /* Traverse directory */
        while ((dir = readdir(dir_stream))) {

            /* Discard all filename that matches regular expression (-x option) */
            if ((dir->d_type == DT_DIR) && excluded(dir->d_name)) {
                continue;
            }
            
            if ((dir->d_type == DT_DIR)
                && strcmp(dir->d_name, ".") != 0
                && strcmp(dir->d_name, "..") != 0)
            {
                /* Absolute path to watch */
                char *path_to_watch = (char *) malloc(strlen(p) + strlen(dir->d_name) + 2);
                strcpy(path_to_watch, p);
                strcat(path_to_watch, dir->d_name);
                strcat(path_to_watch, "/");
                
                /* Append to watched resources */
                add_to_watch_list(path_to_watch, NULL);
				                
                /* Continue directory traversing */
                if (recursive_flag == TRUE) {
                    list_push(list, (void*) path_to_watch);
                }
            } else if (dir->d_type == DT_LNK && nosymlink_flag == FALSE) {
                /* Resolve symbolic link */
                char *symlink = (char *) malloc(strlen(p) + strlen(dir->d_name) + 2);
                strcpy(symlink, p);
                strcat(symlink, dir->d_name);
                strcat(symlink, "/");
                
                char *real_path = resolve_real_path(symlink);
                
                if (real_path != NULL && opendir(real_path) != NULL) {
                    /* Append to watched resources */
                    add_to_watch_list(real_path, symlink);
                    
                    /* Continue directory traversing */
                    if (recursive_flag == TRUE) {
                        list_push(list, (void*) real_path);
                    }
                }
            }
        }
        closedir(dir_stream);
    }
    
    list_free(list);
    
    return 0;
}

LIST_NODE *add_to_watch_list(char *real_path, char *symlink)
{
    /* Check if the resource is already in the watch_list */
    LIST_NODE *node = get_from_path(real_path);
    
    /* If the resource is not watched yet, then add it into the watch_list */
    if (node == NULL) {
        /* Append directory to watch_list */
        int wd = inotify_add_watch(fd, real_path, event_mask);
        
        /* INFO Check limit in: /proc/sys/fs/inotify/max_user_watches */
        if (wd == -1) {
            printf("AN ERROR OCCURRED WHILE ADDING PATH %s:\n", real_path);
            printf("Please consider these possibilities:\n");
            printf(" - Max number of watched resources reached! See /proc/sys/fs/inotify/max_user_watches\n");
            printf(" - Resource is no more available!?\n");
            return NULL;
        }
        
        /* Create the entry */
        WD_DATA *wd_data = malloc(sizeof(WD_DATA));
        wd_data->wd = wd;
        wd_data->path = real_path;
        wd_data->links = list_init();
        wd_data->symbolic_link = (strncmp(root_path, real_path, strlen(root_path)) == 0) ? FALSE : TRUE;
        
        node = list_push(list_wd, (void*) wd_data);
        
        /* Log Message */
        char *message = (char *) malloc(MAXPATHLEN);
        sprintf(message, "WATCHING: (fd:%d,wd:%d)\t\t\"%s\"", fd, wd_data->wd, real_path);
        log_message(message);
    }

    /*
     * Check if the symbolic link (if any) is not been added before.
     * This control is necessary to avoid that cyclic links counts twice.
     */
    if (node != NULL && symlink != NULL) {
        WD_DATA *wd_data = (WD_DATA*) node->data;

        bool_t found = FALSE;
        LIST_NODE *node_link = wd_data->links->first;
        while (node_link) {
            char *link = (char *) node_link->data;
            if (strcmp(link, symlink) == 0) {
                found = TRUE;
                break;
            }
            
            node_link = node_link->next;
        }
        
        if (found == FALSE) {
            list_push(wd_data->links, (void *) symlink);
            /* Log Message */
            char *message = (char *) malloc(MAXPATHLEN);
            sprintf(message, "ADDED SYMBOLIC LINK:\t\t\"%s\" -> \"%s\"", symlink, real_path);
            log_message(message);
        }
    }
    
    return node;
}

void unwatch(char *path, bool_t is_link)
{
    /* Remove the resource from watched resources */
    if (is_link == FALSE) {
        /* Retrieve the watch descriptor from path */
        LIST_NODE *node = get_from_path(path);
        if (node != NULL) {   
            WD_DATA *wd_data = (WD_DATA *) node->data;
            
            /* Log Message */
            char *message = (char *) malloc(MAXPATHLEN);
            sprintf(message, "UNWATCHING: (fd:%d,wd:%d)\t\t%s", fd, wd_data->wd, path);
            log_message(message);
            
            inotify_rm_watch(fd, wd_data->wd);
            
            if (wd_data->links->first != NULL)
                list_free(wd_data->links);
            
            list_remove(list_wd, node);
        }
    } else {
        /* Remove a symbolic link from watched resources */
        LIST_NODE *node = list_wd->first;
        while (node) {
            WD_DATA *wd_data = (WD_DATA*) node->data;
            
            LIST_NODE *link_node = wd_data->links->first;
            while (link_node) {
                char *p = (char*) link_node->data;
                
                /* Symbolic link match. Remove it! */
                if (strcmp(path, p) == 0) {
                    /* Log Message */
                    char *message = (char *) malloc(MAXPATHLEN);
                    sprintf(message, "UNWATCHING SYMBOLIC LINK: \t\t%s -> %s", path, wd_data->path);
                    log_message(message);
                    
                    list_remove(wd_data->links, link_node);
                    
                    /*
                     * if there is no other symbolic links that point to the
                     * watched resource and the watched resource is not the root,
                     * then unwatch it
                     */
                    if (wd_data->links->first == NULL && wd_data->symbolic_link == TRUE) {
                        WD_DATA *sub_wd_data;
                        
                        /*
                         * Build temporary look-up list of resources
                         * that are pointed by some symbolic links.
                         */
                        LIST *tmp_linked_path = list_init();
                        LIST_NODE *sub_node = list_wd->first;
                        while (sub_node) {
                            sub_wd_data = (WD_DATA*) sub_node->data;
                            
                            /*
                             * If it is a PARENT or CHILD and it is referenced by some symbolic link
                             * and it is not listed into tmp_linked_path, then
                             * add it into the list and move to the next resource.
                             */
                            if ((strncmp(wd_data->path, sub_wd_data->path, strlen(wd_data->path)) == 0
                                || strncmp(wd_data->path, sub_wd_data->path, strlen(sub_wd_data->path)) == 0)
                                && sub_wd_data->links->first != NULL
                                && exists(sub_wd_data->path, tmp_linked_path) == FALSE)
                            {
                                /* Save current path into linked_path */
                                list_push(tmp_linked_path, (void*) sub_wd_data->path);
                            }
                            
                            /* Move to next resource */
                            sub_node = sub_node->next;
                        }
                        
                        /*
                         * Descend to all subdirectories of wd_data->path and unwatch them all
                         * only if they or it's parents are not pointed by some symbolic link, anymore
                         * (check if parent is not pointed by symlinks too).
                         */
                        sub_node = list_wd->first;
                        while (sub_node) {
                            sub_wd_data = (WD_DATA*) sub_node->data;
                                                        
                            /*
                             * If it is a CHILD and is NOT referenced by some symbolic link
                             * and it is not listed into tmp_linked_path, then
                             * remove the watch descriptor from the resource.
                             */
                            if (strcmp(root_path, sub_wd_data->path) != 0
                                && strncmp(wd_data->path, sub_wd_data->path, strlen(wd_data->path)) == 0
                                && sub_wd_data->links->first == NULL
                                && exists(sub_wd_data->path, tmp_linked_path) == FALSE)
                            {
                                /* Log Message */
                                char *message = (char *) malloc(MAXPATHLEN);
                                sprintf(message, "UNWATCHING: (fd:%d,wd:%d)\t\t%s", fd, sub_wd_data->wd, sub_wd_data->path);
                                log_message(message);
                                
                                inotify_rm_watch(fd, sub_wd_data->wd);
                                list_remove(list_wd, sub_node);
                            }
                            
                            /* Move to next resource */
                            sub_node = sub_node->next;
                        }
                        
                        /* Free temporay lookup list */
                        list_free(tmp_linked_path);
                    }
                    return;
                }
                link_node = link_node->next;
            }
            node = node->next;
        }
    }
}

bool_t exists(char* child_path, LIST *parents)
{
    if (parents == NULL || parents->first == NULL)
        return FALSE;
    
    LIST_NODE *node = parents->first;
    while(node) {
        char* parent_path = (char*) node->data;
        /* printf("Checking for: %s \t Possible parent: %s\n", child_path, parent_path); */
        if (strlen(parent_path) <= strlen(child_path)
            && strncmp(parent_path, child_path, strlen(parent_path)) == 0)
        {
            return TRUE; /* match! */
        }
        node = node->next;
    }
    return FALSE;
}

bool_t excluded(char *str)
{
    if (NULL == exclude_regex)
        return FALSE;
    
    if (regexec(exclude_regex, str, 0, NULL, 0) == 0)
        return TRUE;

    return FALSE;
}

int monitor()
{
    /* Initialize patterns that will be replaced */
    COMMAND_PATTERN_ROOT = bfromcstr("%r");
    COMMAND_PATTERN_PATH = bfromcstr("%p");
    COMMAND_PATTERN_FILE = bfromcstr("%f");
    COMMAND_PATTERN_EVENT = bfromcstr("%e");
    
    /* Buffer for File Descriptor */
    char buffer[EVENT_BUF_LEN];

    /* inotify_event */
    struct inotify_event *event = NULL;
    struct event_t *triggered_event = NULL;

    /* The real path of touched directory or file */
    char *path = NULL;
    size_t len;
    int i;
    
    /* Temporary node information */
    LIST_NODE *node = NULL;
    WD_DATA *wd_data = NULL;
    
    /* Wait for events */
    while ((len = read(fd, buffer, EVENT_BUF_LEN))) {
        if (len < 0) {
            printf("ERROR: UNABLE TO READ INOTIFY QUEUE EVENTS!!!\n");
            exit(1);
        }
        
        /* index of the event into file descriptor */
        i = 0;
        while (i < len) {
            /* inotify_event */
            event = (struct inotify_event*) &buffer[i];

            /* Discard all filename that matches regular expression (-x option) */
            if (excluded(event->name))
            {
                /* Next event */
                i += EVENT_SIZE + event->len;
                continue;
            }
            
            /* Build the full path of the directory or symbolic link */
            node = get_from_wd(event->wd);
            if (node != NULL) {
                wd_data = (WD_DATA *) node->data;
                path = malloc(strlen(wd_data->path) + strlen(event->name) + 2);
                strcpy(path, wd_data->path);
                strcat(path, event->name);
                if (event->mask & IN_ISDIR)
                    strcat(path, "/");
            } else {
                /* Next event */
                i += EVENT_SIZE + event->len;
                continue;
            }
            
            /* Call the specific event handler */
            if (event->mask & event_mask
                && (triggered_event = get_inotify_event(event->mask & event_mask)) != NULL
                && triggered_event->name != NULL)
            {
                if (triggered_event->handler(event, path) == 0) {
                    if (execute_command(triggered_event->name, path, wd_data->path) == -1) {
                        printf("ERROR OCCURED: Unable to execute the specified command!\n");
                        exit(1);
                    }
                }
            }
            
            /* Next event */
            free(path);
            i += EVENT_SIZE + event->len;
        }
    }

    return 0;
}

int execute_command_inline(char *event_name, char *event_path, char *event_p_path)
{   
    /* For log purpose */
    char *message = (char *) malloc(MAXPATHLEN);
    
    /* Execute the command */
    pid_t pid = fork();
    if (pid > 0) {
        /* parent process */
        sprintf(message, "EVENT TRIGGERED [%s] IN %s\nPROCESS EXECUTED [pid: %d command: %s]",
                event_name, event_path, pid, command->data);
        log_message(message); 
    } else if (pid == 0) {
        /* child process */
        
        /* Command token replacement */
        tmp_command = bfromcstr((char *) command->data);
        bfindreplace(tmp_command, COMMAND_PATTERN_ROOT, bfromcstr(root_path), 0);
        bfindreplace(tmp_command, COMMAND_PATTERN_PATH, bfromcstr(event_p_path), 0);
        bfindreplace(tmp_command, COMMAND_PATTERN_FILE, bfromcstr(event_path), 0);
        bfindreplace(tmp_command, COMMAND_PATTERN_EVENT, bfromcstr(event_name), 0);
	
        /* Fix folder name with spaces */
        bfindreplace(tmp_command, bfromcstr("\\ "), bfromcstr("%T"), 0);
        
        /* Splitting command */
        split_command = bsplit(tmp_command, ' ');
        
        /* Prepare the array to pass to execvp */
        char **command_to_execute = (char **) malloc(sizeof(char *) * (split_command->qty + 1));
        int i;
		
        for (i = 0; i < split_command->qty; ++i) {
            bstring arg = bfromcstr((char *) split_command->entry[i]->data);
            bfindreplace(arg, bfromcstr("%T"), bfromcstr("\ "), 0);
            command_to_execute[i] = arg->data;
        }
        command_to_execute[i] = NULL;
		
        /* exec the command */
        if (execvp(command_to_execute[0], command_to_execute) == -1) {
            sprintf(message, "Unable to execute the specified command!");
            log_message(message);
        }
	    
        /* Free memory */
        free(command_to_execute);
    	bstrListDestroy(split_command);
    	bdestroy(tmp_command);
    } else {
        /* error occured */
        sprintf(message, "ERROR during the fork() !!!");
        log_message(message);
        exit(1);
    }

    return 0;
}

int execute_command_embedded(char *event_name, char *event_path, char *event_p_path)
{
    /* For log purpose */
    char *message = (char *) malloc(MAXPATHLEN);
    
    sprintf(message, "EVENT TRIGGERED [%s] IN %s", event_name, event_path);
    log_message(message);

    /* Output the formatted string */
    tmp_command = bfromcstr((char *) format->data);
    bfindreplace(tmp_command, COMMAND_PATTERN_ROOT, bfromcstr(root_path), 0);
    bfindreplace(tmp_command, COMMAND_PATTERN_PATH, bfromcstr(event_p_path), 0);
    bfindreplace(tmp_command, COMMAND_PATTERN_FILE, bfromcstr(event_path), 0);
    bfindreplace(tmp_command, COMMAND_PATTERN_EVENT, bfromcstr(event_name), 0);
    fprintf(stdout, "%s\n", (char *) tmp_command->data);
    fflush(stdout);
    
    return 0;
}

struct event_t *get_inotify_event(const uint32_t event_mask)
{
    switch (event_mask) {
    case IN_CLOSE:       return &events_lut[32];
    case IN_MOVE:        return &events_lut[33];
    case IN_ALL_EVENTS:  return &events_lut[34];
    default:             return &events_lut[ffs(event_mask)-1];
    }
}

/*
 * EVENT HANDLER IMPLEMENTATION
 */

int event_handler_undefined(struct inotify_event *event, char *path)
{
    return 0;
}

int event_handler_create(struct inotify_event *event, char *path)
{
    /* Return 0 if recurively monitoring is disabled */
    if (recursive_flag == FALSE)
        return 0;
    
    /* Check for a directory */
    if (event->mask & IN_ISDIR) {
        watch(path, NULL);
    } else if (nosymlink_flag == FALSE) {
        /* Check for a symbolic link */
        bool_t is_dir = FALSE;
        DIR *dir_stream = opendir(path);
        if (dir_stream != NULL)
            is_dir = TRUE;
        closedir(dir_stream);
                    
        if (is_dir == TRUE) {
            /* resolve symbolic link */
            char *real_path = resolve_real_path(path);
                        
            /* check if the real path is already monitored */
            LIST_NODE *node = get_from_path(real_path);
            if (node == NULL) {
                watch(real_path, path);
            } else {
                /* 
                 * Append the new symbolic link
                 * to the watched resource
                 */
                WD_DATA *wd_data = (WD_DATA *) node->data;
                list_push(wd_data->links, (void*) path);
                            
                /* Log Message */
                char *message = (char *) malloc(MAXPATHLEN);
                sprintf(message, "ADDED SYMBOLIC LINK:\t\t\"%s\" -> \"%s\"", path, real_path);
                log_message(message);
            }
        }
    }

    return 0;
}

int event_handler_delete(struct inotify_event *event, char *path)
{
    /* Check if it is a folder. If yes unwatch it */
    if (event->mask & IN_ISDIR) {
        unwatch(path, FALSE);
    } else if (nosymlink_flag == FALSE) {
        /*
         * XXX Since it is not possible to know if the
         *     inotify event belongs to a file or a symbolic link,
         *     the unwatch function will be called for each file.
         *     That is because the file is deleted from filesystem,
         *     so there is no way to stat it.
         *     This is a big computational issue to be treated.
         */
        unwatch(path, TRUE);
    }

    return 0;
}

int event_handler_moved_from(struct inotify_event *event, char *path)
{
    return event_handler_delete(event, path);
}

int event_handler_moved_to(struct inotify_event *event, char *path)
{
    if (strncmp(path, root_path, strlen(root_path)) == 0)
        return event_handler_create(event, path);
    
    return 0; /* do nothing */
}
