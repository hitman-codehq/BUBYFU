
#ifndef SCANNER_H
#define SCANNER_H

/* Definitions of the number of arguments that can be passed in and their offsets in the */
/* argument array */

#define ARGS_SOURCE 0
#define ARGS_DEST 1
#define ARGS_ALTDEST 2
#define ARGS_COPY 3
#define ARGS_DELETE 4
#define ARGS_DELETEDIRS 5
#define ARGS_NOCASE 6
#define ARGS_NODEST 7
#define ARGS_NOERRORS 8
#define ARGS_NOHIDDEN 9
#define ARGS_NOPROTECT 10
#define ARGS_NORECURSE 11
#define ARGS_NUM_ARGS 12

/* Forward declarations */

class TEntry;
class TEntryArray;

/* A class for scanning two directories for directory and file entries, and checking to see that */
/* the contents of one directory matches the other.  This process is performed recursively. */

class RScanner
{
	bool	m_bBreakPrinted;	/* true if an error has been printed for ctrl-c */

	int CopyFile(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry);

	int CopyDirectory(char *a_pcSource, char *a_pcDest);

	int CompareDirectories(char *a_pcSource, char *a_pcDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries);

	int CompareFiles(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries);

	int CreateDirectoryTree(char *a_pcPath);

	int DeleteFile(const char *a_pccFileName);

	int	DeleteDir(const char *a_pccPath);

	char *ExtractDirectory(char *a_pcPath);

public:

	RScanner() { m_bBreakPrinted = false;}

	char *QualifyFileName(const char *a_pccDirectoryName, const char *a_pccFileName);

	int Scan(char *a_pcSource, char *a_pcDest);
};

#endif /* ! SCANNER_H */
