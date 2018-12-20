# bigfiles(1)

# Name 

bigfiles - find big files in a directory

# Synopsis

```
bigfiles [ -d "days" ] [ -n "num" ] [ -q ] [ -u "[uid|name]" ] [ -x ] [ -v ] [ directory ]
```

# Description

*bigfiles* recursively searches a directory for big files. It prints the
total number of files encountered; the total space in bytes which the
files occupy; and the names, sizes, and  modification dates of the *num*
largest files encountered (default is 15). If no directory is specified
and the **-q** flag is not set, **bigfiles** searches the current
directory.

Files which have not been accessed within a fixed time period are marked
with an asterisk.

# Options


* **-d***days*  
  Mark, with an asterisk, the names of files which have not been accessed
  in *days* days.  The default value for *days* is 30.
* **-n***num*  
  Print the names of the largest *num* files found.  The default
  value for *num* is 15.
* **-q**  
  Display any applicable disk quota which is in effect for the user's
  home directory, and report the total size of the files encountered as a
  percent of this total. When the **-q** flag is used, searches for files
  always start at the home directory of the user running the program,
  even if some other directory name is given on the command line. The
  **-q** flag automatically turns on the **-u** flag to look at only
  those files owned by the user.
* **-u***[uid|name]*
  With no argument, will look at only those files owned by the user. An
  optional numeric uid argument will look at only those files owned by
  that uid. An argument beginning with an alphabetic character is assumed
  to be a user name, and is converted to a uid restriction. Note that
  with **-u** in effect, all searchable directories are still examined,
  regardless of ownership.
* **-x**
  Restrict the search to the file system containing the directory
  specified.
* **-v**
  Be verbose; shows all directory names as they are being searched.
* *directory*
  Start the search in *directory*; ignored if used with the **-q** flag.

# Notes

If the command is interrupted with a keyboard-interrupt (usually
control-C), it will display the *num* largest files found up to that
point, and then prompt to continue. A "y" will continue the search; a "v"
will toggle the state of verboseness; any other character will terminate
the program.

# Author

Phil Spector <spector@edithst.com>
