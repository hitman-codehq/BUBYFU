
#ifndef SCANNER_H
#define SCANNER_H

/* Definitions of the number of arguments that can be passed in and their offsets in the */
/* argument array */

#define ARGS_SOURCE 0
#define ARGS_DEST 1
#define ARGS_FILELIST 2
#define ARGS_ALTDEST 3
#define ARGS_COPY 4
#define ARGS_DELETE 5
#define ARGS_DELETEDIRS 6
#define ARGS_NOCASE 7
#define ARGS_NODEST 8
#define ARGS_NOERRORS 9
#define ARGS_NOHIDDEN 10
#define ARGS_NOPROTECT 11
#define ARGS_NORECURSE 12
#define ARGS_NUM_ARGS 13

/* Forward declarations */

class TEntry;
class TEntryArray;

/* Each directory or file pattern that can be excluded is represented by an instance */
/* of this class in the exclusion list */

class TExclusion
{
public:

	StdListNode<TExclusion>	m_oStdListNode;		/* Standard list node */
	const char				*m_pccName;			/* Directory or file pattern to be excluded */

	TExclusion(const char *a_pccName)
	{
		m_pccName = a_pccName;
	}

	~TExclusion()
	{
		delete [] (char *) m_pccName;
	}
};

/* A class for scanning two directories for directory and file entries, and checking to see that */
/* the contents of one directory matches the other.  This process is performed recursively. */

class RScanner
{
private:

	bool				m_bBreakPrinted;	/* true if an error has been printed for ctrl-c */
	StdList<TExclusion>	m_oDirectories;		/* List of directories to be excluded */
	StdList<TExclusion>	m_oFiles;			/* List of files to be excluded */

private:

	int AddExclusion(char *a_pcLine);

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

	int Open();

	void Close();

	char *QualifyFileName(const char *a_pccDirectoryName, const char *a_pccFileName);

	int Scan(char *a_pcSource, char *a_pcDest);
};

#endif /* ! SCANNER_H */
