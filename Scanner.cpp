
#include <StdFuncs.h>
#include <Args.h>
#include <Dir.h>
#include <File.h>
#include <FileUtils.h>
#include <StdTextFile.h>
#include <StdWildcard.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include "Scanner.h"

using namespace std;

extern volatile bool g_bBreak;	/* Set to true if when ctrl-c is hit by the user */
extern RArgs g_oArgs;			/* Contains the parsed command line arguments */

/* # of bytes to read and write when copying files */

#define BUFFER_SIZE (1024 * 1024)

/* Written: Wednesday 21-Jul-2010 8:38 am */

int RScanner::open()
{
	bool Inclusion;
	char *Line, *LineCopy;
	const char *FilterListName, *NewLine;
	int RetVal;

	/* Initialise the CRC class's lookup tables */

	m_oCRC.Init();

	/* If the name of a file to use as the filter list has been passed in then open and parse it */

	if ((FilterListName = g_oArgs[ARGS_FILTERLIST]) != NULL)
	{
		RTextFile TextFile;

		/* Read the filter list into memory */

		if ((RetVal = TextFile.open(FilterListName)) == KErrNone)
		{
			/* Scan through and extract the lines from the filter list and build the filter lists */

			while ((NewLine = TextFile.GetLine()) != NULL)
			{
				/* Make a copy of the line returned as the one returned is read only */

				if ((Line = LineCopy = Utils::DuplicateString(NewLine, -1)) != NULL)
				{
					/* Remove white space from the start and end of the string */

					Utils::TrimString(Line);

					/* Normalise the path so it only contains the '/' directory separator */

					Utils::NormalisePath(Line);

					/* Check for a filter */

					if ((*Line == '-') || (*Line == '+'))
					{
						Inclusion = (*Line == '+');

						/* Skip the '-' and remove any further white space after it */

						++Line;
						Utils::TrimString(Line);

						/* Append the filter to the filter list */

						if ((RetVal = AddFilter(Line, Inclusion)) != KErrNone)
						{
							break;
						}
					}

					/* Check to see if this is a comment and if not, display an error */

					else if (*Line != '#')
					{
						printf("Warning: Unknown filter list entry: %s\n", Line);
					}

					/* And free the copy of the line */

					delete [] LineCopy;
				}
				else
				{
					RetVal = KErrNoMemory;

					Utils::Error("Unable to duplicate filter list entry");

					break;
				}
			}

			TextFile.close();
		}
		else
		{
			Utils::Error("Unable to open filter list \"%s\"", FilterListName);
		}
	}

	/* Otherwise don't do anything and just return success */

	else
	{
		RetVal = KErrNone;
	}

	return(RetVal);
}

/* Written: Wednesday 21-Jul-2010 8:40 am */

void RScanner::close()
{
	TFilter *Filter;

	/* Iterate through the items in the path filter list and delete them */

	while ((Filter = m_oPaths.remHead()) != NULL)
	{
		delete Filter;
	}

	/* Iterate through the items in the directory filter list and delete them */

	while ((Filter = m_oDirectories.remHead()) != NULL)
	{
		delete Filter;
	}

	/* Iterate through the items in the file filter wildcard list and delete them */

	while ((Filter = m_oFiles.remHead()) != NULL)
	{
		delete Filter;
	}

	/* And reset the class to is initial state for reuse */

	m_bBreakPrinted = false;
	m_poLastFilter = NULL;
}

/* Written: Wednesday 04-Aug-2010 10:18 am */

int RScanner::AddFilter(char *a_pcLine, bool a_bInclusion)
{
	char *Path;
	const char *FileName;
	int Index, NumSlashes, RetVal;
	size_t Length;
	TFilter *Filter;

	/* Assume failure */

	RetVal = KErrNoMemory;

	/* Allocate a buffer large enough to hold the filter string and a list node */
	/* to hold it */

	Length = strlen(a_pcLine);
	NumSlashes = 0;

	if ((Path = new char[Length + 1]) != NULL)
	{
		if ((Filter = new TFilter(Path)) != NULL)
		{
			RetVal = KErrNone;

			/* See if the filter is a filename wildcard and if so add it to the file filter wildcard list */

			FileName = Utils::filePart(a_pcLine);

			if (*FileName != '\0')
			{
				strcpy(Path, FileName);

				if (a_bInclusion)
				{
					/* File inclusion filters can only can only get added as embedded filters */

					if (m_poLastFilter)
					{
						m_poLastFilter->m_oFilters.addTail(Filter);

						if (g_oArgs[ARGS_VERBOSE]) printf("  Added inclusion filter \"%s\" to directory filter \"%s\"\n", Path, m_poLastFilter->m_pccName);
					}
					else
					{
						printf("Warning: Ignoring file filter \"%s\"\n", Path);

						delete Filter;
					}
				}
				else
				{
					m_oFiles.addTail(Filter);

					if (g_oArgs[ARGS_VERBOSE]) printf("Added file filter \"%s\"\n", Path);
				}
			}

			/* Otherwise add it to the directory list */

			else
			{
				if (!(a_bInclusion))
				{
					for (Index = 0; Index < (int) Length; ++Index)
					{
						if (a_pcLine[Index] == '/')
						{
							++NumSlashes;
						}
					}

					/* First, remove the trailing '/' that is appended to the directory name to be filtered */

					a_pcLine[Length - 1] = '\0';

					/* And add it to the directory or path list as appropriate */

					strcpy(Path, a_pcLine);

					if (NumSlashes >= 2)
					{
						m_oPaths.addTail(Filter);
					}
					else
					{
						m_oDirectories.addTail(Filter);
					}

					/* And save the ptr to the directory filter so that embedded filters can be added to it l8r */

					m_poLastFilter = Filter;

					if (g_oArgs[ARGS_VERBOSE]) printf("Added directory filter \"%s\"\n", a_pcLine);
				}
				else
				{
					printf("  Warning: Ignoring inclusion filter \"%s\"\n", a_pcLine);

					delete Filter;
				}
			}
		}
		else
		{
			delete [] Path;

			Utils::Error("Out of memory");
		}
	}
	else
	{
		Utils::Error("Out of memory");
	}

	return(RetVal);
}

/**
 * Calculates the CRC of two files and determines whether they match.
 * This function will parse two files and will calculate the CRC of both, to determine whether
 * they are a binary match.  It returns whether the files match and also the calculated CRC values.
 * If any error occurs (apart from the CRC values not matching), the returned CRC values will set to 0.
 *
 * @date    Saturday 17-May-2014 11:16 am, Code HQ Ehinger Tor
 * @param	a_pccSource		Ptr to the name of the source file to be checked
 * @param	a_pccDest		Ptr to the name of the destination file to be checked
 * @param	a_puiSourceCRC	Ptr to a variable into which to return the CRC of the source file
 * @param	a_puiDestCRC	Ptr to a variable into which to return the CRC of the destination file
 * @return	KErrNone if the CRCs of the two files match
 * @return	KErrCorrupt if the CRCs of the two files do not match
 * @return	KErrNotFound if either the source or the destination files could not be opened
 * @return	Otherwise any of the errors returned by RFile::open() or RFile::read()
 */

TInt RScanner::CheckCRC(const char *a_pccSource, const char *a_pccDest, TUint *a_puiSourceCRC, TUint *a_puiDestCRC)
{
	unsigned char *Source, *Dest;
	TInt RetVal, SourceSize, DestSize;
	TUint SourceCRC, DestCRC;
	RFile SourceFile, DestFile;

	SourceCRC = DestCRC = 0;
	*a_puiSourceCRC = *a_puiDestCRC = 0;

	/* Open the source and destination files which need to have their CRCs checked */

	if ((RetVal = SourceFile.open(a_pccSource, EFileRead)) == KErrNone)
	{
		if ((RetVal = DestFile.open(a_pccDest, EFileRead)) == KErrNone)
		{
			/* Allocate two large buffers into which chunks of the source and destination files can be read */

			Source = new unsigned char[BUFFER_SIZE];
			Dest = new unsigned char[BUFFER_SIZE];

			if ((Source) && (Dest))
			{
				/* Loop around and read the two files in, in chunks of BUFFER_SIZE, incrementally calculating their CRCs. */
				/* Stop when the entire file has been read in and checked or if an error occurs */

				do
				{
					SourceSize = SourceFile.read(Source, BUFFER_SIZE);

					if (SourceSize > 0)
					{
						if ((DestSize = DestFile.read(Dest, SourceSize)) == SourceSize)
						{
							/* Incrementally calculate the two CRCs, based on the CRC of the previous chunk */

							SourceCRC = m_oCRC.CRC32(SourceCRC, Source, SourceSize);
							DestCRC = m_oCRC.CRC32(DestCRC, Dest, DestSize);
						}
						else
						{
							/* If the amount read from the destination file read was less than what was requested, then we */
							/* have hit the end of the file so return this fact.  Otherwise return whatever error was returned */
							/* from RFile::read() */

							RetVal = (DestSize >= 0) ? KErrGeneral : DestSize;

							Utils::Error("Unable to read from file \"%s\" (Error %d)", a_pccDest, RetVal);
						}
					}
					else if (SourceSize < 0)
					{
						RetVal = SourceSize;

						Utils::Error("Unable to read from file \"%s\" (Error %d)", a_pccSource, RetVal);
					}
				}
				while ((SourceSize > 0) && (RetVal == KErrNone));

				/* If the CRCs were calculated successfully then return whether they match, and also return the CRCs themselves */

				if (RetVal == KErrNone)
				{
					if (SourceCRC != DestCRC)
					{
						RetVal = KErrCorrupt;
					}

					*a_puiSourceCRC = SourceCRC;
					*a_puiDestCRC = DestCRC;
				}
			}
			else
			{
				RetVal = KErrNoMemory;

				Utils::Error("Out of memory");
			}

			delete [] Dest;
			delete [] Source;
			DestFile.close();
		}
		else
		{
			Utils::Error("Unable to open dest file \"%s\" (Error %d)", a_pccDest, RetVal);
		}

		SourceFile.close();
	}
	else
	{
		Utils::Error("Unable to open source file \"%s\" (Error %d)", a_pccSource, RetVal);
	}

	return(RetVal);
}

/* Written: Saturday 23-Oct-2010 11:27 am */

bool RScanner::CheckFilterList(const char *a_pccFileName)
{
	bool RetVal;
	const char *FileName;
	TFilter *Filter;

	/* Assume the file is not on the filter list */

	RetVal = false;

	/* Get the name of the file part of the path */

	FileName = Utils::filePart(a_pccFileName);

	/* Iterate through the list of file filters and see if there is a match */

	if ((Filter = m_oFiles.getHead()) != NULL)
	{
		do
		{
			/* Perform a wildcard match of the file filter on the filename */

			RWildcard Wildcard;

			if (Wildcard.open(Filter->m_pccName) == KErrNone)
			{
				/* If the file matches the file filter then we want to bail out and not copy */
				/* the file */

				if (Wildcard.Match(FileName))
				{
					RetVal = true;

					break;
				}

				Wildcard.close();
			}
		}
		while ((Filter = m_oFiles.getSucc(Filter)) != NULL);
	}

	return(RetVal);
}

/* Written: Friday 02-Jan-2009 8:38 pm */

int RScanner::CopyFile(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry)
{
	int RetVal;
	unsigned char *Buffer;
	RFile SourceFile, DestFile;

	/* Assume success */

	RetVal = KErrNone;

	printf("Copying file \"%s\" to \"%s\"\n", a_pccSource, a_pccDest);

	if ((RetVal = SourceFile.open(a_pccSource, EFileRead)) == KErrNone)
	{
		/* RFile::Create() will fail if a destination file already exists so delete it before trying */
		/* to create a new one.  RScanner::deleteFile() will also remove any read only protection bit */
		/* that is set before trying to delete the file */

		if ((RetVal = deleteFile(a_pccDest)) == KErrNotFound)
		{
			RetVal = KErrNone;
		}

		if (RetVal == KErrNone)
		{
			if ((RetVal = DestFile.create(a_pccDest, EFileWrite)) == KErrNone)
			{
				if ((Buffer = new unsigned char[BUFFER_SIZE]) != NULL)
				{
					do
					{
						if ((RetVal = SourceFile.read(Buffer, BUFFER_SIZE)) > 0)
						{
							if ((RetVal = DestFile.write(Buffer, RetVal)) < 0)
							{
								Utils::Error("Unable to write to file \"%s\" (Error %d)", a_pccDest, RetVal);
							}
						}
						else
						{
							if (RetVal < 0)
							{
								Utils::Error("Unable to read from file \"%s\" (Error %d)", a_pccSource, RetVal);
							}
						}
					}
					while (RetVal > 0);

					delete [] Buffer;
				}
				else
				{
					RetVal = KErrNoMemory;

					Utils::Error("Out of memory");
				}

				DestFile.close();
			}
			else
			{
				Utils::Error("Unable to open dest file \"%s\" (Error %d)", a_pccDest, RetVal);
			}
		}
		else
		{
			Utils::Error("Unable to delete dest file \"%s\" (Error %d)", a_pccDest, RetVal);
		}

		SourceFile.close();

		/* If successful, set the date and time and protection bits in the destination file to match those */
		/* in the source file */

		if (RetVal == KErrNone)
		{
			if ((RetVal = Utils::setFileDate(a_pccDest, a_roEntry, EFalse)) == KErrNone)
			{
				if ((RetVal = Utils::setProtection(a_pccDest, a_roEntry.iAttributes)) == KErrNone)
				{

#ifdef WIN32

					/* If requested, set the archive attribute on the source file to indicate that it has been */
					/* archived.  This attribute exists only on Windows, so to prevent unnecessarily setting the */
					/* protection bits on onther systems we do this conditionally */

					if (g_oArgs[ARGS_ARCHIVE])
					{
						TEntry Entry = a_roEntry;

						/* Clear the archive attribute */

						Entry.ClearArchive();

						/* And write the new protection bits back to the source file */

						if ((RetVal = Utils::setProtection(a_pccSource, Entry.iAttributes)) != KErrNone)
						{
							Utils::Error("Unable to set protection bits for file \"%s\" (Error %d)", a_pccSource, RetVal);
						}
					}

#endif /* WIN32 */

				}
				else
				{
					Utils::Error("Unable to set protection bits for file \"%s\" (Error %d)", a_pccDest, RetVal);
				}
			}
			else
			{
				Utils::Error("Unable to set datestamp for file \"%s\" (Error %d)", a_pccDest, RetVal);
			}
		}
	}
	else
	{
		Utils::Error("Unable to open source file \"%s\" (Error %d)", a_pccSource, RetVal);
	}

	if (g_oArgs[ARGS_NOERRORS])
	{
		RetVal = KErrNone;
	}

	return(RetVal);
}

/**
 * Copies a file or a link to a destination.
 * Depending on whether the file system object given is a file or a link, this function will call
 * the appropriate function to perform a copy of the object.  This is just a convenience function
 * as both the CopyFile() and CopyLink() functions are quite large and do not belong in a single
 * function.
 *
 * @date	Monday 30-Nov-2015 07:05 am, Code HQ Ehinger Tor
 * @param	a_pccSource		Pointer to the name of the source file or link
 * @param	a_pccDest		Pointer to the name of the destination file
 * @param	a_roEntry		Reference to information about the object to be copied
 * @return	One of the return values of CopyFile() or CopyLink()
 */

int RScanner::CopyFileOrLink(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry)
{
	int RetVal;

	if (a_roEntry.IsLink())
	{
		RetVal = CopyLink(a_pccSource, a_pccDest, a_roEntry);
	}
	else
	{
		RetVal = CopyFile(a_pccSource, a_pccDest, a_roEntry);
	}

	return(RetVal);
}

/* Written: Saturday 03-Jan-2009 8:42 am */

int RScanner::CopyDirectory(char *a_pcSource, char *a_pcDest)
{
	int RetVal;

	printf("Copying directory \"%s\" to \"%s\"\n", a_pcSource, a_pcDest);

	/* Create the new subdirectory in the target directory */

	if ((RetVal = CreateDirectoryTree(a_pcDest)) == KErrNone)
	{
		TEntry Entry;

		/* If CreateDirectoryTree() returned KErrNone then either the directory was created successfully */
		/* or no creation was required (for instance, the user passed in x:\).  In this case we want to */
		/* ensure that the directory actually exists so that don't get into a loop of trying to create it */

		if ((RetVal = Utils::GetFileInfo(a_pcDest, &Entry)) == KErrNone)
		{
			/* Call scan on the source and new destination directory, which will automatically copy all files */
			/* found in the source directory, and recurse into any directories found */

			if ((RetVal = Scan(a_pcSource, a_pcDest)) == KErrNone)
			{
				/* Now give the destination directory the same timestamp as the source one */

				if ((RetVal = Utils::GetFileInfo(a_pcSource, &Entry)) == KErrNone)
				{
					if ((RetVal = Utils::setFileDate(a_pcDest, Entry)) != KErrNone)
					{
						Utils::Error("Unable to set datestamp for directory \"%s\" (Error = %d)", a_pcDest, RetVal);
					}
				}
				else
				{
					Utils::Error("Unable to get file information for directory \"%s\" (Error = %d)", a_pcSource, RetVal);
				}
			}
		}
		else
		{
			Utils::Error("Target directory \"%s\" does not exist (Error = %d)", a_pcDest, RetVal);
		}
	}
	else
	{
		Utils::Error("Unable to create directory \"%s\" (Error = %d)", a_pcDest, RetVal);
	}

	return(RetVal);
}

/**
 * Copies a link to a destination.
 * This function will create a link in the destination given by a_pccDest that is a
 * clone of the link given by a_pccSource.  The function will determine the relative
 * path to the source link and will recreate that relative path on the destination
 * link, thus allowing links to be recreated on different drives.  For example, if
 * the source link points to Work:Some/Directory/File.txt and it is being copied to
 * Ram: then the newly created link will point to Ram:Some/Directory/File.txt.
 *
 * This function only handles links where the file being linked to is in the same
 * directory as the link being linked from.
 *
 * @date	Monday 30-Nov-2015 07:10 am, Code HQ Ehinger Tor
 * @param	a_pccSource		Pointer to the name of the source link
 * @param	a_pccDest		Pointer to the name of the destination link
 * @param	a_roEntry		Reference to information about the object to be copied
 * @return	KErrNone if successful
 * @return	KErrGeneral if the link could not be created
 * @return	KErrNoMemory if not enough memory was available
 */

int RScanner::CopyLink(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry)
{
	char *ResolvedSourceFile, *ResolvedSourcePath;
	const char *SourceSlash, *DestSlash;
	int RetVal;
	size_t Offset;
	string SourcePath, DestPath;
	TEntry Entry;

	/* Assume success */

	RetVal = KErrNone;

	printf("Copying link \"%s\" to \"%s\"\n", a_pccSource, a_pccDest);

	/* Determine the path of the source link, without a trailing '/' */

	SourcePath = a_pccSource;
	SourceSlash = Utils::filePart(SourcePath.c_str());

	Offset = (SourceSlash - SourcePath.c_str());

	if ((Offset > 0) && (SourcePath[Offset - 1] == '/'))
	{
		--Offset;
	}

	SourcePath.resize(Offset);

	/* Determine the path of the destination link, without a trailing '/' */

	DestPath = a_pccDest;
	DestSlash = Utils::filePart(DestPath.c_str());

	Offset = (DestSlash - DestPath.c_str());

	if ((Offset > 0) && (DestPath[Offset - 1] == '/'))
	{
		--Offset;
	}

	DestPath.resize(Offset);

	/* Now determine the real name and path of the source link;  that is, the name and path of the */
	/* file that the source link points to.  This will be used for determining the name of the file */
	/* that the desination link will point to */

	ResolvedSourceFile = Utils::ResolveFileName(a_pccSource);
	ResolvedSourcePath = Utils::ResolveFileName(SourcePath.c_str());

	if ((ResolvedSourceFile) && (ResolvedSourcePath))
	{
		/* Assuming that the target of the link exists in the same directory as the source link, or */
		/* below, strip out the base path to the source link (keeping the relative path) to obtain the */
		/* relative path to the target file */

		string LinkTarget = ResolvedSourceFile;

		if ((Offset = LinkTarget.rfind(ResolvedSourcePath)) != string::npos)
		{
			LinkTarget.erase(0, (Offset + strlen(ResolvedSourcePath)));
		}

		/* If the path ends in ':' then it will not be included in the filename so we don't need to */
		/* worry about it.  If it ends in '/' then it will be at the start of the filename so we need */
		/* to remove it.  This is because we are handling the paths ourselves in here rather than using */
		/* a helper function */

		if (LinkTarget[0] == '/')
		{
			LinkTarget.erase(0, 1);
		}

		/* Creating a link will fail if it already exists so delete it if necessary.  Don't check for its */
		/* existence first as this is in itself tricky, due to it being a link! */

		deleteFile(a_pccDest);

		/* And finally after much work, create the link to the destination file */

		printf("  %s -> %s\n", a_pccDest, LinkTarget.c_str());
		RetVal = Utils::makeLink(a_pccDest, LinkTarget.c_str());

		if (RetVal == KErrNone)
		{
			if ((RetVal = Utils::setFileDate(a_pccDest, a_roEntry, EFalse)) != KErrNone)
			{
				Utils::Error("Unable to set datestamp on link \"%s\" (Error %d)", a_pccDest, RetVal);
			}
		}
		else
		{
			Utils::Error("Unable to copy link from \"%s\" to \"%s\" (Error %d)", a_pccSource, a_pccDest, RetVal);
		}

		if (g_oArgs[ARGS_NOERRORS])
		{
			RetVal = KErrNone;
		}
	}
	else
	{
		RetVal = KErrNoMemory;
	}

	delete [] ResolvedSourceFile;
	delete [] ResolvedSourcePath;

	return(RetVal);
}

/* Written: Saturday 18-Jul-2009 8:48 pm */

int RScanner::CompareDirectories(char *a_pcSource, char *a_pcDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries)
{
	int RetVal;
	const TEntry *DestEntry;

	/* Scan the source and destination directories and mirror the source into the destination, but only */
	/* if the NORECURSE parameter has not been specified */

	if (!(g_oArgs[ARGS_NORECURSE]))
	{
		RetVal = Scan(a_pcSource, a_pcDest);
	}
	else
	{
		RetVal = KErrNone;
	}

	/* Iterate through the destination list and find the directory we have just mirrored into */

	DestEntry = a_roDestEntries.getHead();

	while (DestEntry)
	{
		if (_stricmp(a_roEntry.iName, DestEntry->iName) == 0)
		{
			/* Remove the entry from the destination list to speed up future searches and facilitate the */
			/* ability to detect files that only exist in the destination directory */

			a_roDestEntries.remove(DestEntry);
			delete (TEntry *) DestEntry;

			break;
		}

		DestEntry = a_roDestEntries.getSucc(DestEntry);
	}

	if (g_oArgs[ARGS_NOERRORS])
	{
		RetVal = KErrNone;
	}

	return(RetVal);
}

/* Written: Saturday 03-Nov-2007 10:31 pm */

int RScanner::CompareFiles(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries)
{
	bool CRCOk, Match, ModifiedOk;
	int SourceMilliSeconds, DestMilliSeconds, RetVal;
	TUint SourceCRC, DestCRC;
	const TEntry *DestEntry;

	/* Assume success */

	RetVal = KErrNone;

	/* Iterate through the entries in the destination directory and see if the source file already */
	/* exists and has the identical propereties */

	DestEntry = a_roDestEntries.getHead();

	while (DestEntry)
	{
		/* Perform a case dependent or non case dependent comparison of the filenames, depending on */
		/* the user's preference */

		if (g_oArgs[ARGS_NOCASE])
		{
			Match = (!(_stricmp(a_roEntry.iName, DestEntry->iName)));
		}
		else
		{
			Match = (!(strcmp(a_roEntry.iName, DestEntry->iName)));
		}

		if (Match)
		{
			/* Only check the modification time if the user has not explicitly disabled */
			/* the checking */

			if (g_oArgs[ARGS_NODATES])
			{
				ModifiedOk = ETrue;
			}
			else
			{
				/* Do a special check of the modification time and date. Some file systems (such as ext2 over */
				/* Samba) are not particularly accurate and their timestamps can be up to a second out, due to */
				/* the file's milliseconds value not being tracked. This is an unfortunately empirical hack but */
				/* it's required for backing up from Amiga OS SFS -> ext2 and NTFS -> ext2 */

				ModifiedOk = (a_roEntry.iModified == DestEntry->iModified) ? ETrue : EFalse;

				if (!(ModifiedOk))
				{
					/* Convert the source and destination times to milliseconds */

					SourceMilliSeconds = ((((a_roEntry.iModified.DateTime().Hour() * 60) +
						a_roEntry.iModified.DateTime().Minute()) * 60) + a_roEntry.iModified.DateTime().Second());
					SourceMilliSeconds = ((SourceMilliSeconds * 1000) + a_roEntry.iModified.DateTime().MilliSecond());

					DestMilliSeconds = ((((DestEntry->iModified.DateTime().Hour() * 60) +
						DestEntry->iModified.DateTime().Minute()) * 60) + DestEntry->iModified.DateTime().Second());
					DestMilliSeconds = ((DestMilliSeconds * 1000) + DestEntry->iModified.DateTime().MilliSecond());

					/* And ensure that there is no more than 1 second difference */

					if (abs(DestMilliSeconds - SourceMilliSeconds) < 1000)
					{
						ModifiedOk = true;
					}
				}
			}

			/* If the user has asked to check the CRC then do that now.  Otherwise just pretend that the CRC is valid */

			SourceCRC = DestCRC = 0;
			CRCOk = true;

			if (g_oArgs[ARGS_CRC])
			{
				/* Calculate the CRCs of the source and destination files */

				RetVal = CheckCRC(a_pccSource, a_pccDest, &SourceCRC, &DestCRC);

				/* If the CRCs match or if they don't match then this is considered a valid result so indicate this. */
				/* Any other return value from CheckCRC() is considered an error and will halt processing */

				if (RetVal == KErrNone)
				{
					CRCOk = true;
				}
				else if (RetVal == KErrCorrupt)
				{
					RetVal = KErrNone;
					CRCOk = false;
				}
			}

			if (RetVal == KErrNone)
			{
				/* Only copy the file or print a message if the file is not on the filter list */

				if (!(CheckFilterList(a_pccSource)))
				{
					/* If the source and destination files are different sizes, or their modification times are */
					/* different, or their attributes are different and the user has not specified the NOPROTECT */
					/* command line option, then the two files are classified as not matching and must be either */
					/* copied or at least information printed about them */

					if ((a_roEntry.iSize != DestEntry->iSize) || (!(ModifiedOk)) || (!(CRCOk)) ||
						((!(g_oArgs[ARGS_NOPROTECT])) && (a_roEntry.iAttributes != DestEntry->iAttributes)))
					{
						/* If the user has specified the COPY command line option then copy the file now */

						if (g_oArgs[ARGS_COPY])
						{
							RetVal = CopyFileOrLink(a_pccSource, a_pccDest, a_roEntry);
						}

						/* Otherwise just display information on why the files do not match, possibly */
						/* repairing their metadata if requested to do so */

						else
						{
							printf("File \"%s\" does not match: ", a_pccSource);

							if (a_roEntry.iSize != DestEntry->iSize)
							{

								/* Amiga OS has no support for the %lld format specifier, so we have no choice but to just */
								/* cast the 64 bit value to an integer and hope for the best */

#ifdef __amigaos__

								printf("size = %d -vs- %d\n", (int) a_roEntry.iSize, (int) DestEntry->iSize);

#else /* ! __amigaos__ */

								printf("size = %lld -vs- %lld\n", a_roEntry.iSize, DestEntry->iSize);

#endif /* ! __amigaos__ */

							}
							else if (!(ModifiedOk))
							{
								std::string SourceDate, SourceTime, DestDate, DestTime;

								Utils::TimeToString(SourceDate, SourceTime, a_roEntry);
								Utils::TimeToString(DestDate, DestTime, *DestEntry);

								/* Set the target file's date and time to match that of the source, if requested */

								if (g_oArgs[ARGS_FIXDATES])
								{
									printf("%s %s -> %s %s\n", SourceDate.c_str(), SourceTime.c_str(), DestDate.c_str(), DestTime.c_str());

									if ((RetVal = Utils::setFileDate(a_pccDest, a_roEntry)) != KErrNone)
									{
										Utils::Error("Unable to set datestamp for file \"%s\" (Error %d)", a_pccDest, RetVal);
									}
								}
								else
								{
									printf("%s %s -vs- %s %s\n", SourceDate.c_str(), SourceTime.c_str(), DestDate.c_str(), DestTime.c_str());
								}
							}
							else if (!(CRCOk))
							{
								printf("CRC = %x -vs- %x\n", SourceCRC, DestCRC);
							}
							else
							{
								/* Set the target file's attributes to match that of the source, if requested */

								if (g_oArgs[ARGS_FIXPROTECT])
								{
									printf("attributes %x -> %x\n", a_roEntry.iAttributes, DestEntry->iAttributes);

									if ((RetVal = Utils::setProtection(a_pccDest, a_roEntry.iAttributes)) != KErrNone)
									{
										Utils::Error("Unable to set protection bits for file \"%s\" (Error %d)", a_pccDest, RetVal);
									}
								}
								else
								{
									printf("attributes = %x -vs- %x\n", a_roEntry.iAttributes, DestEntry->iAttributes);
								}
							}
						}
					}

					/* Remove the entry from the destination list to speed up future searches and facilitate the */
					/* ability to detect files that only exist in the destination directory */

					a_roDestEntries.remove(DestEntry);
					delete (TEntry *) DestEntry;
				}
				else
				{
					if (g_oArgs[ARGS_VERBOSE]) printf("Excluding %s\n", a_pccSource);
				}
			}

			break;
		}

		/* Get the nest destination entry */

		DestEntry = a_roDestEntries.getSucc(DestEntry);
	}

	/* If we have reached the end of the destination filter list without finding a match then the file */
	/* does not exist, so either copy it or print a message, as appropriate */

	if (!(DestEntry))
	{
		/* Only copy the file or print a message if the file is not on the filter list */

		if (!(CheckFilterList(a_pccSource)))
		{
			/* If the user has specified the COPY command line option then copy the file now */

			if (g_oArgs[ARGS_COPY])
			{
				RetVal = CopyFileOrLink(a_pccSource, a_pccDest, a_roEntry);
			}
			else
			{
				printf("File \"%s\" does not exist\n", a_pccDest);
			}
		}
		else
		{
			if (g_oArgs[ARGS_VERBOSE]) printf("Excluding %s\n", a_pccSource);
		}
	}

	return(RetVal);
}

/* Written: Tuesday 29-Dec-2009 9:54 am */

int RScanner::CreateDirectoryTree(char *a_pcPath)
{
	char *Dir, *StartPath, *EndPath;
	int RetVal;
	size_t Length;

	/* Assume success */

	RetVal = KErrNone;

	/* Iterate through the path and extract the individual directory names from it, creating each */
	/* one as we go */

	StartPath = a_pcPath;
	EndPath = (a_pcPath + strlen(a_pcPath));

	while ((Dir = ExtractDirectory(a_pcPath)) != NULL)
	{
		/* If the returned name is not a device or drive, try to create the directory */

		Length = strlen(Dir);

		if ((Length > 0) && (Dir[Length - 1] != ':'))
		{
			/* Create the directory just extracted, using the pointer to the original path so that */
			/* directories within directories are also created.  If the directory already exists then */
			/* this is ok */

			if ((RetVal = Utils::CreateDirectory(StartPath)) == KErrAlreadyExists)
			{
				RetVal = KErrNone;
			}
		}

		if (RetVal == KErrNone)
		{
			/* Calculate the start of the next directory name in the path */

			a_pcPath += Length;

			/* If we are past the end of the path then we have finished */

			if (a_pcPath >= EndPath)
			{
				break;
			}

			/* Otherwise put the separator back in (it will have been removed by ExtractDirectory()) */
			/* and continue iterating */

			else
			{
				*a_pcPath++ = '/';
			}
		}
		else
		{
			break;
		}
	}

	return(RetVal);
}

/* Written: Friday 02-Jan-2009 8:50 pm */

int	RScanner::deleteFile(const char *a_pccFileName)
{
	int RetVal;
	RFileUtils FileUtils;

	/* Try to delete the file */

	if ((RetVal = FileUtils.deleteFile(a_pccFileName)) != KErrNone)
	{
		/* Deleting the file failed.  This may be because it is protected from deletion, so try */
		/* making it deleteable and try again */

		if ((RetVal = Utils::SetDeleteable(a_pccFileName)) == KErrNone)
		{
			RetVal = FileUtils.deleteFile(a_pccFileName);
		}
	}

	if (RetVal != KErrNone)
	{
		if (g_oArgs[ARGS_NOERRORS])
		{
			RetVal = KErrNone;
		}
	}

	return(RetVal);
}

/* Written: Wednesday 15-Jul-2009 6:55 am */

int	RScanner::DeleteDir(const char *a_pccPath)
{
	char *NextEntry;
	int RetVal;
	RDir Dir;
	const TEntry *Entry;
	TEntryArray *EntryArray;

	if ((RetVal = Dir.open(a_pccPath)) == KErrNone)
	{
		if ((RetVal = Dir.read(EntryArray)) == KErrNone)
		{
			NextEntry = NULL;
			Entry = EntryArray->getHead();

			while (Entry)
			{
				if ((NextEntry = QualifyFileName(a_pccPath, Entry->iName)) != NULL)
				{
					if (Entry->IsDir())
					{
						printf("Deleting directory \"%s\"\n", NextEntry);

						if ((RetVal = DeleteDir(NextEntry)) != KErrNone)
						{
							break;
						}
					}
					else
					{
						if ((RetVal = deleteFile(NextEntry)) != KErrNone)
						{
							Utils::Error("Unable to delete file \"%s\" (Error %d)", NextEntry, RetVal);

							if (!(g_oArgs[ARGS_NOERRORS]))
							{
								break;
							}
						}
					}

					delete [] NextEntry;
					NextEntry = NULL;
				}
				else
				{
					RetVal = KErrNoMemory;

					Utils::Error("Out of memory");

					break;
				}

				Entry = EntryArray->getSucc(Entry);
			}

			delete [] NextEntry;
		}
		else
		{
			Utils::Error("Unable to scan directory \"%s\" (Error %d)", a_pccPath, RetVal);

			if (g_oArgs[ARGS_NOERRORS])
			{
				RetVal = KErrNone;
			}
		}

		Dir.close();

		/* The files and subdirectories in the directory have been deleted, so now */
		/* delete the directory itself */

		if (RetVal == KErrNone)
		{
			if ((RetVal = Utils::DeleteDirectory(a_pccPath)) != KErrNone)
			{
				Utils::Error("Unable to delete directory \"%s\" (Error = %d)", a_pccPath, RetVal);

				if (g_oArgs[ARGS_NOERRORS])
				{
					RetVal = KErrNone;
				}
			}
		}
	}
	else
	{
		Utils::Error("Unable to open directory \"%s\" (Error %d)", a_pccPath, RetVal);
	}

	return(RetVal);
}

/* Written: Tuesday 29-Dec-2009 9:58 am */

char *RScanner::ExtractDirectory(char *a_pcPath)
{
	char *RetVal;

	/* Always point to the start of the extracted directory name, unless we are at the end */
	/* of the path */

	RetVal = a_pcPath;

	/* If the path starts with a '/' then we want to skip it rather than return an empty string */

	if (*a_pcPath == '/')
	{
		++a_pcPath;
	}

	/* Find the next path separator or the end of the path */

	while ((*a_pcPath) && (*a_pcPath != '/'))
	{
		++a_pcPath;
	}

	/* If we have found another directory name then NULL terminate it */

	if (a_pcPath != RetVal)
	{
		*a_pcPath = '\0';
	}

	/* Otherwise indicate failure */

	else
	{
		RetVal = NULL;
	}

	return(RetVal);
}

/* Written: Saturday 18-Jul-2009 8:41 am */

char *RScanner::QualifyFileName(const char *a_pccDirectoryName, const char *a_pccFileName)
{
	char *RetVal;
	size_t FileNameLength;

	FileNameLength = (strlen(a_pccDirectoryName) + strlen(a_pccFileName) + 2);

	if ((RetVal = new char[FileNameLength]) != NULL)
	{
		strcpy(RetVal, a_pccDirectoryName);
		Utils::addPart(RetVal, a_pccFileName, FileNameLength);
	}

	return(RetVal);
}

/* Written: Saturday 03-Nov-2007 9:43 pm */

int RScanner::Scan(char *a_pcSource, char *a_pcDest)
{
	bool CheckFile, CopyDir, InclusionsOnly;
	char *NextSource, *NextDest;
	const char *DirectoryName;
	int RetVal;
	RDir SourceDir, DestDir;
	const TEntry *Entry;
	TEntry DirEntry;
	TEntryArray *SourceEntries, *DestEntries;
	TFilter *Filter, *Inclusion;

	/* By default, copy the directory */

	InclusionsOnly = false;
	CopyDir = true;
	RetVal = KErrNone;

	/* Get the name of the last directory or file in the path.  The Utils::filePart() function can be */
	/* used for this as it doesn't know the difference */

	DirectoryName = Utils::filePart(a_pcSource);

	/* Iterate through the list of directory filters and see if there is a match for the source directory */

	if ((Filter = m_oDirectories.getHead()) != NULL)
	{
		do
		{
			/* Perform a wildcard match of the directory filter on the directory name */

			RWildcard Wildcard;

			if (Wildcard.open(Filter->m_pccName) == KErrNone)
			{
				/* If the directory matches the directory filter then we want to bail out and not copy */
				/* the directory, unless the filter also contains an inclusion filter */

				if (Wildcard.Match(DirectoryName))
				{
					if (Filter->m_oFilters.getHead() == NULL)
					{
						if (g_oArgs[ARGS_VERBOSE]) printf("Excluding %s\n", a_pcSource);

						CopyDir = false;
					}
					else
					{
						if (g_oArgs[ARGS_VERBOSE]) printf("Copying %s with inclusions\n", a_pcSource);

						InclusionsOnly = true;
					}

					break;
				}

				Wildcard.close();
			}
		}
		while ((Filter = m_oDirectories.getSucc(Filter)) != NULL);
	}

	/* Now iterate through the list of path filters and see if there is a match for the source directory. */
	/* Only do this if we have not already found an entry in the directory filter list */

	if (!(Filter))
	{
		if ((Filter = m_oPaths.getHead()) != NULL)
		{
			do
			{
				/* Perform a wildcard match of the directory filter on the directory name */

				RWildcard Wildcard;

				if (Wildcard.open(Filter->m_pccName) == KErrNone)
				{
					/* If the directory matches the directory filter then we want to bail out and not copy */
					/* the directory, unless the filter also contains an inclusion filter */

					if (Wildcard.Match(a_pcSource))
					{
						if (Filter->m_oFilters.getHead() == NULL)
						{
							if (g_oArgs[ARGS_VERBOSE]) printf("Excluding %s\n", a_pcSource);

							CopyDir = false;
						}
						else
						{
							if (g_oArgs[ARGS_VERBOSE]) printf("Copying %s with inclusions\n", a_pcSource);

							InclusionsOnly = true;
						}

						break;
					}

					Wildcard.close();
				}
			}
			while ((Filter = m_oPaths.getSucc(Filter)) != NULL);
		}
	}

	/* Copy the directory if required */

	if (CopyDir)
	{
		if ((RetVal = SourceDir.open(a_pcSource)) == KErrNone)
		{
			if ((RetVal = SourceDir.read(SourceEntries)) == KErrNone)
			{
				if ((RetVal = DestDir.open(a_pcDest)) == KErrNone)
				{
					if ((RetVal = DestDir.read(DestEntries)) == KErrNone)
					{
						NextSource = NextDest = NULL;
						Entry = SourceEntries->getHead();

						while (Entry)
						{
							if (Entry->IsHidden())
							{
								CheckFile = (!(g_oArgs[ARGS_NOHIDDEN]));
							}
							else
							{
								CheckFile = true;
							}

							if (CheckFile)
							{
								NextSource = QualifyFileName(a_pcSource, Entry->iName);
								NextDest = QualifyFileName(a_pcDest, Entry->iName);

								if ((NextSource) && (NextDest))
								{
									if (Entry->IsDir())
									{
										RetVal = CompareDirectories(NextSource, NextDest, *Entry, *DestEntries);
									}
									else
									{
										/* If we are copying only inclusions in this directory then scan through the inclusion */
										/* wildcard list for this directory's filter and see if the filename matches any of the */
										/* inclusion wildcards */

										if (InclusionsOnly)
										{
											if ((Inclusion = Filter->m_oFilters.getHead()) != NULL)
											{
												do
												{
													/* Perform a wildcard match of the inclusion filter on the current file */

													RWildcard Wildcard;

													if (Wildcard.open(Inclusion->m_pccName) == KErrNone)
													{
														if (Wildcard.Match(Entry->iName))
														{
															/* The file matches the wildcard so copy it if necessary and break out of the */
															/* loop that is checking the wildcards */

															RetVal = CompareFiles(NextSource, NextDest, *Entry, *DestEntries);

															break;
														}

														Wildcard.close();
													}
												}
												while ((Inclusion = Filter->m_oFilters.getSucc(Inclusion)) != NULL);
											}
										}

										/* We are copying all files so just copy the file if necessary */

										else
										{
											RetVal = CompareFiles(NextSource, NextDest, *Entry, *DestEntries);
										}
									}
								}
								else
								{
									RetVal = KErrNoMemory;

									Utils::Error("Out of memory");
								}

								delete [] NextDest;
								NextDest = NULL;
								delete [] NextSource;
								NextSource = NULL;

								if (g_bBreak)
								{
									/* Because Scan() is recursive, this error can be printed multiple times if we are in a subdirectory */
									/* (once for each recurse of Scan()) so use a flag to ensure that it is only printed once */

									if (!(m_bBreakPrinted))
									{
										m_bBreakPrinted = true;

										printf("BUBYFU: ***Break\n");
									}

									RetVal = KErrCompletion;
								}

								if (RetVal != KErrNone)
								{
									break;
								}
							}

							Entry = SourceEntries->getSucc(Entry);
						}

						delete [] NextDest;
						delete [] NextSource;

						if (RetVal == KErrNone)
						{
							if (DestEntries->Count() > 0)
							{
								Entry = DestEntries->getHead();

								while (Entry)
								{
									if ((NextDest = QualifyFileName(a_pcDest, Entry->iName)) != NULL)
									{
										if (Entry->IsDir())
										{
											if (g_oArgs[ARGS_DELETEDIRS])
											{
												printf("Deleting directory \"%s\"\n", NextDest);

												if ((RetVal = DeleteDir(NextDest)) != KErrNone)
												{
													break;
												}
											}
											else if ((!(g_oArgs[ARGS_NODEST])) && (!(g_oArgs[ARGS_NORECURSE])))
											{
												printf("Directory \"%s\" exists only in destination\n", NextDest);
											}
										}
										else
										{
											if (g_oArgs[ARGS_DELETE])
											{
												printf("Deleting file \"%s\"\n", NextDest);

												if ((RetVal = deleteFile(NextDest)) != KErrNone)
												{
													Utils::Error("Unable to delete file \"%s\" (Error %d)", NextDest, RetVal);

													if (!(g_oArgs[ARGS_NOERRORS]))
													{
														break;
													}
												}
											}
											else if (!(g_oArgs[ARGS_NODEST]))
											{
												printf("File \"%s\" exists only in destination\n", NextDest);
											}
										}

										delete [] NextDest;
										NextDest = NULL;
									}
									else
									{
										RetVal = KErrNoMemory;

										Utils::Error("Out of memory");
									}

									Entry = DestEntries->getSucc(Entry);
								}

								delete [] NextDest;
							}
						}
					}
					else
					{
						Utils::Error("Unable to scan dest directory \"%s\" (Error %d)", a_pcDest, RetVal);
					}

					DestDir.close();
				}
				else
				{
					if ((RetVal == KErrNotFound) && (g_oArgs[ARGS_COPY]))
					{
						// TODO: CAW - Bugger, what to do about this, which causes all directories that contains
						//             exclusion filters get get copied?
						if (InclusionsOnly)
						{
							RetVal = KErrNone;
						}
						else
						{
							RetVal = CopyDirectory(a_pcSource, a_pcDest);
						}
					}
					else
					{
						if (RetVal == KErrNotFound)
						{
							printf("Directory \"%s\" does not exist\n", a_pcDest);
						}
						else
						{
							Utils::Error("Unable to open dest directory \"%s\" (Error %d)", a_pcDest, RetVal);
						}

						//if (g_oArgs[ARGS_NOERRORS])
						{
							RetVal = KErrNone;
						}
					}
				}
			}
			else
			{
				Utils::Error("Unable to scan source directory \"%s\" (Error %d)", a_pcSource, RetVal);
			}

			SourceDir.close();
		}
		else
		{
			Utils::Error("Unable to open source directory \"%s\" (Error %d)", a_pcSource, RetVal);
		}
	}
	else
	{
		/* If the directory was filtered out then we may have to delete it if requested */

		if (Utils::GetFileInfo(a_pcDest, &DirEntry) == KErrNone)
		{
			if (g_oArgs[ARGS_DELETEDIRS])
			{
				printf("Deleting directory \"%s\"\n", a_pcDest);

				RetVal = DeleteDir(a_pcDest);
			}
			else if ((!(g_oArgs[ARGS_NODEST])) && (!(g_oArgs[ARGS_NORECURSE])))
			{
				printf("Directory \"%s\" exists only in destination\n", a_pcDest);
			}
		}
	}

	if (g_oArgs[ARGS_NOERRORS])
	{
		RetVal = KErrNone;
	}

	return(RetVal);
}
