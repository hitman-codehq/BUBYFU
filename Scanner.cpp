
#include <StdFuncs.h>
#include <Args.h>
#include <BaUtils.h>
#include <Dir.h>
#include <File.h>
#include <stdio.h>
#include <string.h>
#include "Scanner.h"

extern volatile bool g_bBreak;	/* Set to true if when ctrl-c is hit by the user */
extern RArgs g_oArgs;			/* Contains the parsed command line arguments */

// TODO: CAW - Comment entire file
// TODO: CAW - Think about how to handle links
// TODO: CAW - Look into the use of NOERRORS + this breaks ctrl-c + are files left open after ctrl-c?
// TODO: CAW - Finish the ALTDEST stuff or get rid of it

/* # of bytes to read and write when copying files */

#define BUFFER_SIZE (1024 * 1024)

/* Written: Friday 02-Jan-2009 8:38 pm */

int RScanner::CopyFile(const char *a_pccSource, const char *a_pccDest, const TEntry &a_roEntry)
{
	int RetVal;
	unsigned char *Buffer;
	RFile SourceFile, DestFile;

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

			/* Remove the entry from the destination list to speed up future searches and facilitate the */
			/* ability to detect files that only exist in the destination directory */

			a_roDestEntries.Remove(DestEntry);
			delete DestEntry;

			break;
		}

		/* Get the nest destination entry */

		DestEntry = a_roDestEntries.GetSucc(DestEntry);
	}

	/* If we have reached the end of the destination file list without finding a match then the file */
	/* does not exist, so either copy it or print a message, as appropriate */

	if (!(DestEntry))
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
	bool CheckFile;
	char *NextSource, *NextDest;
	int RetVal;
	RDir SourceDir, DestDir;
	const TEntry *Entry;
	TEntryArray *SourceEntries, *DestEntries;

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
									RetVal = CompareFiles(NextSource, NextDest, *Entry, *DestEntries);
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
					RetVal = CopyDirectory(a_pcSource, a_pcDest);
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

	return(RetVal);
}
