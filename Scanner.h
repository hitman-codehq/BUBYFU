
#ifndef SCANNER_H
#define SCANNER_H

#include <StdCRC.h>

/* Definitions of the number of arguments that can be passed in and their offsets in the */
/* argument array */

#define ARGS_SOURCE 0
#define ARGS_DEST 1
#define ARGS_FILTERLIST 2
#define ARGS_ARCHIVE 3
#define ARGS_COPY 4
#define ARGS_CRC 5
#define ARGS_DELETE 6
#define ARGS_DELETEDIRS 7
#define ARGS_FIXDATES 8
#define ARGS_FIXPROTECT 9
#define ARGS_NOCASE 10
#define ARGS_NODATES 11
#define ARGS_NODEST 12
#define ARGS_NOERRORS 13
#define ARGS_NOHIDDEN 14
#define ARGS_NOPROTECT 15
#define ARGS_NORECURSE 16
#define ARGS_VERBOSE 17
#define ARGS_NUM_ARGS 18

/* Forward declarations */

class TEntry;
class TEntryArray;

/* Each directory or file pattern that can be filtered is represented by an instance */
/* of this class in the filter list */

class TFilter
{
public:

	StdListNode<TFilter>	m_oStdListNode;		/* Standard list node */
	StdList<TFilter>		m_oFilters;			/* List of filters to be recursively applied */
	const char				*m_pccName;			/* Directory or file pattern to use as filter */

	TFilter(const char *a_pccName)
	{
		m_pccName = a_pccName;
	}

	~TFilter()
	{
		TFilter *Filter;

		/* Iterate through the items in the directory filter list and delete them */

		while ((Filter = m_oFilters.remHead()) != NULL)
		{
			delete Filter;
		}

		/* And delete the filter's name */

		delete [] (char *) m_pccName;
	}
};

/* A class for scanning two directories for directory and file entries, and checking to see that */
/* the contents of one directory matches the other.  This process is performed recursively */

class RScanner
{
private:

	bool				m_bBreakPrinted;	/* true if an error has been printed for ctrl-c */
	StdList<TFilter>	m_oDirectories;		/* List of directories to be filtered out */
	StdList<TFilter>	m_oFiles;			/* List of files to be filtered out */
	StdList<TFilter>	m_oPaths;			/* List of paths to be filtered out */
	RCRC				m_oCRC;				/* Class with which to generate CRC values */
	TFilter				*m_poLastFilter;	/* Ptr to last directory filter, if any */

private:

	int AddFilter(char *a_pcLine, bool a_bInclusion);

	TInt CheckCRC(const char *a_pccSource, const char *a_pccDest, TUint *a_puiSourceCRC, TUint *a_puiDestCRC);

	bool CheckFilterList(const char *a_pccFileName);

	int CopyFile(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry);

	int CopyFileOrLink(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry);

	int CopyLink(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry);

	int CopyDirectory(char *a_pcSource, char *a_pcDest);

	int CompareDirectories(char *a_pcSource, char *a_pcDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries);

	int CompareFiles(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries);

	int CreateDirectoryTree(char *a_pcPath);

	int deleteFile(const char *a_pccFileName);

	int	DeleteDir(const char *a_pccPath);

	char *ExtractDirectory(char *a_pcPath);

public:

	RScanner()
	{
		m_bBreakPrinted = false;
		m_poLastFilter= NULL;
	}

	int open();

	void close();

	char *QualifyFileName(const char *a_pccDirectoryName, const char *a_pccFileName);

	int Scan(char *a_pcSource, char *a_pcDest);
};

#endif /* ! SCANNER_H */
