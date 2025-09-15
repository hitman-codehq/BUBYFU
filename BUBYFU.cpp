
#include <StdFuncs.h>
#include <Args.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "Scanner.h"

#ifdef __amigaos4__

/* Lovely version structure.  Only Amiga makes it possible! */

static const struct Resident g_oROMTag __attribute__((used)) =
{
	RTC_MATCHWORD,
	(struct Resident *) &g_oROMTag,
	(struct Resident *) (&g_oROMTag + 1),
	RTF_AUTOINIT,
	0,
	NT_LIBRARY,
	0,
	"BUBYFU",
	"\0$VER: BUBYFU 0.02 (23.10.2010)\r\n",
	NULL
};

/* Use a large stack, as copying is highly recursive and can use a lot of stack space */

static const char __attribute__((used)) g_accStackCookie[] = "$STACK:262144";

#endif /* __amigaos4__ */

/* Template for use in obtaining command line parameters.  Remember to change the indexes */
/* in Scanner.h if the ordering or number of these change */

static const char g_accTemplate[] = "SOURCE/A,DEST/A,FILTERLIST,ARCHIVE/S,COPY/S,CRC/S,DELETE/S,DELETEDIRS/S,FIXDATES/S,FIXPROTECT/S,NOCASE/S,NODATES/S,NODEST/S,NOERRORS/S,NOHIDDEN/S,NOPROTECT/S,NORECURSE/S,VERBOSE/S";

volatile bool g_bBreak;		/* Set to true if when ctrl-c is hit by the user */
RArgs g_oArgs;				/* Contains the parsed command line arguments */

/* Written: Friday 02-Jan-2009 10:30 am */

static void SignalHandler(int /*a_iSignal*/)
{
	/* Signal that ctrl-c has been pressed so that we break out of the scanning routine */

	g_bBreak = true;
}

int main(int a_iArgC, char *a_ppcArgV[])
{
	char *Source, *Dest;
	size_t Length;
	TInt Result;
	RScanner Scanner;

	/* Install a ctrl-c handler so we can handle ctrl-c being pressed and shut down the scan */
	/* properly */

	signal(SIGINT, SignalHandler);

	/* Parse the command line parameters passed in and make sure they are formatted correctly */

	if ((Result = g_oArgs.open(g_accTemplate, ARGS_NUM_ARGS, a_iArgC, a_ppcArgV)) == KErrNone)
	{
		/* Open the scanner and allow it to parse the filter list if it exists.  It will display */
		/* any errors required */

		if (Scanner.open() == KErrNone)
		{
			/* RScanner::Scan() is able to modify the parameters passed in so make a copy of the */
			/* source and destination paths before proceeding */

			Source = Scanner.QualifyFileName("", g_oArgs[ARGS_SOURCE]);
			Dest = Scanner.QualifyFileName("", g_oArgs[ARGS_DEST]);

			if ((Source) && (Dest))
			{
				/* Normalise the paths so they only contain the '/' directory separator.  This will */
				/* make comparing against the filter list easier */

				Utils::NormalisePath(Source);
				Utils::NormalisePath(Dest);

				/* Strip any trailing '/' separators, so that calls to Utils::GetFileInfo() do not fail */

				Length = strlen(Source);

				if ((Length > 0) && (Source[Length - 1] == '/'))
				{
					Source[Length - 1] = '\0';
				}

				Length = strlen(Dest);

				if ((Length > 0) && (Dest[Length - 1] == '/'))
				{
					Dest[Length - 1] = '\0';
				}

				Result = Scanner.Scan(Source, Dest);
			}
			else
			{
				Utils::Error("Out of memory");
			}

			delete Dest;
			delete Source;

			Scanner.close();
		}

		g_oArgs.close();
	}
	else
	{
		if (Result == KErrNotFound)
		{
			Utils::Error("Required argument missing");
		}
		else
		{
			Utils::Error("Unable to read command line arguments");
		}
	}

	return((Result == KErrNone) ? RETURN_OK : RETURN_ERROR);
}
