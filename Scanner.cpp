
#include <StdFuncs.h>
#include <Args.h>
#include <BaUtils.h>
#include <Dir.h>
#include <File.h>
#include <StdTextFile.h>
#include <StdWildcard.h>
#include <stdio.h>
#include <string.h>
#include "Scanner.h"

extern volatile bool g_bBreak;	/* Set to true if when ctrl-c is hit by the user */
extern RArgs g_oArgs;			/* Contains the parsed command line arguments */

// TODO: CAW - Comment entire file
// TODO: CAW - Think about how to handle links
// TODO: CAW - Look into the use of NOERRORS + this breaks ctrl-c + are files left open after ctrl-c?
// TODO: CAW - Finish the ALTDEST stuff or get rid of it
// TODO: CAW - Add proper support for wildcards for both directories and files
// TODO: CAW - If you add a directory to the exclude list after copying then it won't get deleted
// TODO: CAW - Calling Utils::GetFileInfo() causes memory leaks as the user has to free TEntry::iName!

/* # of bytes to read and write when copying files */

#define BUFFER_SIZE (1024 * 1024)

/* Written: Wednesday 21-Jul-2010 8:38 am */

int RScanner::Open()
{
	bool Inclusion;
	char *Line;
	const char *FilterListName;
	int RetVal;

	/* If the name of a file to use as the filter list has been passed in then open and parse it */

	if ((FilterListName = g_oArgs[ARGS_FILTERLIST]) != NULL)
	{
		RTextFile TextFile;

		/* Read the filter list into memory */

		if ((RetVal = TextFile.Open(FilterListName)) == KErrNone)
		{
			/* Scan through and extract the lines from the filter list and build the filter lists */

			while ((Line = TextFile.GetLine()) != NULL)
			{
				/* Remove white space from the start and end of the string */

				Utils::TrimString(Line);

				/* Check for a filter */

				if ((*Line == '-') || (*Line == '+'))
				{
					Inclusion = (*Line == '+');

					/* Skip the '-' and remove and further white space after it */

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
			}

			TextFile.Close();
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

void RScanner::Close()
{
	TFilter *Filter;

	/* Iterate through the items in the directory filter list and delete them */

	while ((Filter = m_oDirectories.RemHead()) != NULL)
	{
		delete Filter;
	}

	/* Iterate through the items in the file filter wildcard list and delete them */

	while ((Filter = m_oFiles.RemHead()) != NULL)
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
	int RetVal;
	TFilter *Filter;

	/* Assume failure */

	RetVal = KErrNoMemory;

	/* Allocate a buffer large enough to hold the filter string and a list node */
	/* to hold it */

	if ((Path = new char[strlen(a_pcLine) + 1]) != NULL)
	{
		if ((Filter = new TFilter(Path)) != NULL)
		{
			RetVal = KErrNone;

			/* See if the filter is a filename wildcard and if so add it to the file filter wildcard list */

			FileName = Utils::FilePart(a_pcLine);

			if (*FileName != '\0')
			{
				strcpy(Path, FileName);

				if (a_bInclusion)
				{
					/* File inclusion filters can only can only get added as embedded filters */

					if (m_poLastFilter)
					{
						m_poLastFilter->m_oFilters.AddTail(Filter);

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
					m_oFiles.AddTail(Filter);

					if (g_oArgs[ARGS_VERBOSE]) printf("Added file filter \"%s\"\n", Path);
				}
			}

			/* Otherwise add it to the directory list */

			else
			{
				if (!(a_bInclusion))
				{
					/* First, remove the trailing '/' that is appended to the directory name to be filtered */

					a_pcLine[strlen(a_pcLine) - 1] = '\0';

					/* And add it to the directory list */

					strcpy(Path, a_pcLine);
					m_oDirectories.AddTail(Filter);

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

/* Written: Saturday 23-Oct-2010 11:27 am */

bool RScanner::CheckFilterList(const char *a_pccFileName)
{
	bool RetVal;
	const char *Extension, *SourceExtension;
	TFilter *Filter;

	/* Assume the file is not on the filter list */

	RetVal = false;

	/* See if the filename contains an extension and if so, iterate through the list of file filter */
	/* wildcards and see if there is a match */

	if ((SourceExtension = Utils::Extension(a_pccFileName)) != NULL)
	{
		if ((Filter = m_oFiles.GetHead()) != NULL)
		{
			do
			{
				/* Extract the extension of the current filter wildcard */

				if ((Extension = Utils::Extension(Filter->m_pccName)) != NULL)
				{
					/* If this matches the current file then bail out and don't copy the file */

					if (strcmp(Extension, SourceExtension) == 0)
					{
						RetVal = true;

						break;
					}
				}
			}
			while ((Filter = m_oFiles.GetSucc(Filter)) != NULL);
		}
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

	if ((RetVal = SourceFile.Open(a_pccSource, EFileRead)) == KErrNone)
	{
		/* RFile::Create() will fail if a destination file already exists so delete it before trying */
		/* to create a new one.  RScanner::DeleteFile() will also remove any read only protection bit */
		/* that is set before trying to delete the file */

		if ((RetVal = DeleteFile(a_pccDest)) == KErrNotFound)
		{
			RetVal = KErrNone;
		}

		if (RetVal == KErrNone)
		{
			if ((RetVal = DestFile.Create(a_pccDest, EFileWrite)) == KErrNone)
			{
				if ((Buffer = new unsigned char[BUFFER_SIZE]) != NULL)
				{
					do
					{
						if ((RetVal = SourceFile.Read(Buffer, BUFFER_SIZE)) > 0)
						{
							if ((RetVal = DestFile.Write(Buffer, RetVal)) < 0)
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

				DestFile.Close();
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

		SourceFile.Close();

		/* If successful, set the date and time and protection bits in the destination file to match those */
		/* in the source file */

		if (RetVal == KErrNone)
		{
			if ((RetVal = Utils::SetFileDate(a_pccDest, a_roEntry)) == KErrNone)
			{
				if ((RetVal = Utils::SetProtection(a_pccDest, a_roEntry.iAttributes)) != KErrNone)
				{
					Utils::Error("Unable to set protection bits on file \"%s\" (Error %d)", a_pccDest, RetVal);
				}
			}
			else
			{
				Utils::Error("Unable to set datestamp on file \"%s\" (Error %d)", a_pccDest, RetVal);
			}
		}
	}
	else
	{
		Utils::Error("Unable to open source file \"%s\" (Error %d)", a_pccSource, RetVal);

		if (g_oArgs[ARGS_NOERRORS])
		{
			RetVal = KErrNone;
		}
	}

	return(RetVal);
}

/* Written: Saturday 03-Jan-2009 8:42 am */

int RScanner::CopyDirectory(char *a_pcSource, char *a_pcDest)
{
	int RetVal;
	TEntry Entry;

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
					if ((RetVal = Utils::SetFileDate(a_pcDest, Entry)) != KErrNone)
					{
						Utils::Error("Unable to set file information for directory \"%s\" (Error = %d)", a_pcDest, RetVal);
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

/* Written: Saturday 18-Jul-2009 8:48 pm */

int RScanner::CompareDirectories(char *a_pcSource, char *a_pcDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries)
{
	int RetVal;
	const TEntry *DestEntry;

	/* Scan the source and destination directories and mirror the source into the destination */

	if (!(g_oArgs[ARGS_NORECURSE]))
	{
		RetVal = Scan(a_pcSource, a_pcDest);
	}
	else
	{
		TEntry Entry;

		if (Utils::GetFileInfo(a_pcDest, &Entry) == KErrNone)
		{
			RetVal = (Entry.iIsDir) ? KErrNone : KErrGeneral;
		}
		else
		{
			RetVal = KErrNotFound;
		}
	}

	if (RetVal == KErrNone)
	{
		/* Iterate through the destination list and find the directory we have just mirrored into */

		DestEntry = a_roDestEntries.GetHead();

		while (DestEntry)
		{
			if (stricmp(a_roEntry.iName, DestEntry->iName) == 0)
			{
				/* Remove the entry from the destination list to speed up future searches and facilitate the */
				/* ability to detect files that only exist in the destination directory */

				a_roDestEntries.Remove(DestEntry);
				delete DestEntry;

				break;
			}

			DestEntry = a_roDestEntries.GetSucc(DestEntry);
		}
	}

	//if (g_oArgs[ARGS_NOERRORS]) This is obsolete + how does it interact with the above KErrNotFound?
	{
		RetVal = KErrNone;
	}

	return(RetVal);
}

/* Written: Saturday 03-Nov-2007 10:31 pm */

int RScanner::CompareFiles(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry, TEntryArray &a_roDestEntries)
{
	bool Match, ModifiedOk;
	int SourceSeconds, DestSeconds, RetVal;
	const TEntry *DestEntry;

	/* Assume success */

	RetVal = KErrNone;

	/* Iterate through the entries in the destination directory and see if the source file already */
	/* exists and has the identical propereties */

	DestEntry = a_roDestEntries.GetHead();

	while (DestEntry)
	{
		/* Perform a case dependent or non case dependent comparison of the filenames, depending on */
		/* the user's preference */

		if (g_oArgs[ARGS_NOCASE])
		{
			Match = (!(stricmp(a_roEntry.iName, DestEntry->iName)));
		}
		else
		{
			Match = (!(strcmp(a_roEntry.iName, DestEntry->iName)));
		}

		if (Match)
		{
			/* Do a special check of the modification time and date. Some file systems (such as ext2 over */
			/* Samba) are not particularly accurate and their timestamps can be a second out. This is an */
			/* unfortunately empirical hack but it's required for backing up from AmigaOS SFS => ext2 */

			ModifiedOk = (a_roEntry.iModified == DestEntry->iModified) ? ETrue : EFalse;

			if (!(ModifiedOk))
			{
				/* Convert the source and destination times to seconds */

				SourceSeconds = ((((a_roEntry.iModified.DateTime().Hour() * 60) + a_roEntry.iModified.DateTime().Minute()) * 60) +
					a_roEntry.iModified.DateTime().Second());

				DestSeconds = ((((DestEntry->iModified.DateTime().Hour() * 60) + DestEntry->iModified.DateTime().Minute()) * 60) +
					DestEntry->iModified.DateTime().Second());

				/* And ensure that they are no more than 1 second different */

				if (abs(DestSeconds - SourceSeconds) == 1)
				{
					ModifiedOk = true;
				}
			}

			/* If the source and destination files are different sizes, or their modification times are */
			/* different, or their attributes are different and the user has not specified the NOPROTECT */
			/* command line option, then the two files are classified as not matching and must be either */
			/* copied or at least information printed about them */

			if ((a_roEntry.iSize != DestEntry->iSize) || (!(ModifiedOk)) ||
				((!(g_oArgs[ARGS_NOPROTECT])) && (a_roEntry.iAttributes != DestEntry->iAttributes)))
			{
				/* Only copy the file or print a message if the file is not on the filter list */

				if (!(CheckFilterList(a_pccSource)))
				{
					/* If the user has specified the COPY command line option then copy the file now */

					if (g_oArgs[ARGS_COPY])
					{
						RetVal = CopyFile(a_pccSource, a_pccDest, a_roEntry);
					}

					/* Otherwise just display information on why the files do not match */

					else
					{
						printf("File \"%s\" does not match: ", a_pccSource);

						if (a_roEntry.iSize != DestEntry->iSize)
						{
							printf("size = %d -vs- %d\n", a_roEntry.iSize, DestEntry->iSize);
						}
						else if (!(ModifiedOk))
						{
							char SourceDate[20], SourceTime[20], DestDate[20], DestTime[20];

							Utils::TimeToString(SourceDate, SourceTime, a_roEntry);
							Utils::TimeToString(DestDate, DestTime, *DestEntry);

							printf("%s %s -vs- %s %s\n", SourceDate, SourceTime, DestDate, DestTime);
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

			a_roDestEntries.Remove(DestEntry);
			delete DestEntry;

			break;
		}

		/* Get the nest destination entry */

		DestEntry = a_roDestEntries.GetSucc(DestEntry);
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
				RetVal = CopyFile(a_pccSource, a_pccDest, a_roEntry);
			}
			else
			{
				printf("File \"%s\" does not exist\n", a_pccDest);
			}
		}
	}

	return(RetVal);
}

/* Written: Tuesday 29-Dec-2009 9:54 am */

int RScanner::CreateDirectoryTree(char *a_pcPath)
{
	char *Dir, *StartPath, *EndPath;
	int Length, RetVal;

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

// TODO: CAW - This is bodgey as it is Windows specific + should reset attributes on new file to match
//             those of old file.  Windows stuff should be in StdFuncs - check Cleaner & ls as well
int	RScanner::DeleteFile(const char *a_pccFileName)
{
	int RetVal;

	if ((RetVal = BaflUtils::DeleteFile(a_pccFileName)) != KErrNone)
	{

#ifndef __amigaos4__

		if ((RetVal = Utils::SetProtection(a_pccFileName, FILE_ATTRIBUTE_NORMAL)) == KErrNone)
		{
			RetVal = BaflUtils::DeleteFile(a_pccFileName);
		}

#endif /* ! __amigaos4__ */

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

	if ((RetVal = Dir.Open(a_pccPath)) == KErrNone)
	{
		if ((RetVal = Dir.Read(EntryArray)) == KErrNone)
		{
			Entry = EntryArray->GetHead();

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
						if ((RetVal = DeleteFile(NextEntry)) != KErrNone)
						{
							Utils::Error("Unable to delete file \"%s\" (Error %d)", NextEntry, RetVal);

							if (!(g_oArgs[ARGS_NOERRORS]))
							{
								delete [] NextEntry;

								break;
							}
						}
					}

					delete [] NextEntry;
				}
				else
				{
					RetVal = KErrNoMemory;

					Utils::Error("Out of memory");

					break;
				}

				Entry = EntryArray->GetSucc(Entry);
			}
		}
		else
		{
			Utils::Error("Unable to scan directory \"%s\" (Error %d)", a_pccPath, RetVal);

			if (g_oArgs[ARGS_NOERRORS])
			{
				RetVal = KErrNone;
			}
		}

		Dir.Close();

		if (RetVal == KErrNone)
		{
			if ((RetVal = DeleteFile(a_pccPath)) != KErrNone)
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

// TODO: CAW - Make a test case for this, including testing for "\"
char *RScanner::ExtractDirectory(char *a_pcPath)
{
	char *RetVal;

	/* Always point to the start of the extracted directory name, unless we are at the end */
	/* of the path */

	RetVal = a_pcPath;

	/* If the path starts with a '\' or '/' then we want to skip it rather than return an empty */
	/* string */

	if ((*a_pcPath != '/') || (*a_pcPath != '\\'))
	{
		++a_pcPath;
	}

	/* Find the next path separator or the end of the path */

	while ((*a_pcPath) && (*a_pcPath != '/') && (*a_pcPath != '\\'))
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
	int FileNameLength;

	FileNameLength = (strlen(a_pccDirectoryName) + strlen(a_pccFileName) + 2);

	if ((RetVal = new char[FileNameLength]) != NULL)
	{
		strcpy(RetVal, a_pccDirectoryName);
		Utils::AddPart(RetVal, a_pccFileName, FileNameLength);
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
	TEntryArray *SourceEntries, *DestEntries;
	TFilter *Filter, *Inclusion;

	/* By default, copy the directory */

	InclusionsOnly = false;
	CopyDir = true;
	RetVal = KErrNone;

	/* Get the name of the last directory or file in the path.  The Utils::FilePart() function can be */
	/* used for this as it doesn't know the difference */

	DirectoryName = Utils::FilePart(a_pcSource);

	/* Iterate through the list of directory filters and see if there is a match for the source directory */

	if ((Filter = m_oDirectories.GetHead()) != NULL)
	{
		do
		{
			/* Perform a wildcard match of the directory filter on the directory name */

			RWildcard Wildcard;

			if (Wildcard.Open(Filter->m_pccName) == KErrNone)
			{
				/* If the directory matches the directory filter then we want to bail out and not copy */
				/* the directory, unless the filter also contains an inclusion filter */

				if (Wildcard.Match(DirectoryName))
				{
					if (Filter->m_oFilters.GetHead() == NULL)
					{
						if (g_oArgs[ARGS_VERBOSE]) printf("Excluding %s\n", a_pcSource);

						CopyDir = false;
					}
					else
					{
						if (g_oArgs[ARGS_VERBOSE]) printf("Copying %s with inclusions\n", a_pcSource);

						InclusionsOnly = true;
					}
				}

				Wildcard.Close();
			}
		}
		while ((Filter = m_oDirectories.GetSucc(Filter)) != NULL);
	}

	/* Copy the directory if required */

	if (CopyDir)
	{
		if ((RetVal = SourceDir.Open(a_pcSource)) == KErrNone)
		{
			if ((RetVal = SourceDir.Read(SourceEntries)) == KErrNone)
			{
				if ((RetVal = DestDir.Open(a_pcDest)) == KErrNone)
				{
					if ((RetVal = DestDir.Read(DestEntries)) == KErrNone)
					{
						Entry = SourceEntries->GetHead();

						while (Entry)
						{
							CheckFile = false;

							if (!(Entry->IsLink()))
							{
								if (Entry->IsHidden())
								{
									CheckFile = (!(g_oArgs[ARGS_NOHIDDEN]));
								}
								else
								{
									CheckFile = true;
								}
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
											if ((Inclusion = Filter->m_oFilters.GetHead()) != NULL)
											{
												do
												{
													/* Perform a wildcard match of the inclusion filter on the current file */

													RWildcard Wildcard;

													if (Wildcard.Open(Inclusion->m_pccName) == KErrNone)
													{
														if (Wildcard.Match(Entry->iName))
														{
															/* The file matches the wildcard so copy it if necessary and break out of the */
															/* loop that is checking the wildcards */

															RetVal = CompareFiles(NextSource, NextDest, *Entry, *DestEntries);

															break;
														}

														Wildcard.Close();
													}
												}
												while ((Inclusion = Filter->m_oFilters.GetSucc(Inclusion)) != NULL);
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
								delete [] NextSource;

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

							Entry = SourceEntries->GetSucc(Entry);
						}

						if (RetVal == KErrNone)
						{
							if (DestEntries->Count() > 0)
							{
								Entry = DestEntries->GetHead();

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
										else if (!(Entry->IsLink()))
										{
											if (g_oArgs[ARGS_DELETE])
											{
												printf("Deleting file \"%s\"\n", NextDest);

												if ((RetVal = DeleteFile(NextDest)) != KErrNone)
												{
													Utils::Error("Unable to delete file \"%s\" (Error %d)", NextDest, RetVal);

													if (!(g_oArgs[ARGS_NOERRORS]))
													{
														delete [] NextDest;

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
									}
									else
									{
										RetVal = KErrNoMemory;

										Utils::Error("Out of memory");
									}

									Entry = DestEntries->GetSucc(Entry);
								}
							}
						}
					}
					else
					{
						Utils::Error("Unable to scan dest directory \"%s\" (Error %d)", a_pcDest, RetVal);
					}

					DestDir.Close();
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

			SourceDir.Close();
		}
		else
		{
			Utils::Error("Unable to open source directory \"%s\" (Error %d)", a_pcSource, RetVal);

			if (g_oArgs[ARGS_NOERRORS])
			{
				RetVal = KErrNone;
			}
		}
	}

	return(RetVal);
}
