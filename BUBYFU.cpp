
#include <StdFuncs.h>
#include <Args.h>
#include <stdio.h>
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
	"\0$VER: BUBYFU 0.02 (28.12.2009)\r\n",
	NULL
};

#endif /* __amigaos4__ */

/* Template for use in obtaining command line parameters.  Remember to change the indexes */
/* in Scanner.h if the ordering or number of these change */

static const char g_accTemplate[] = "SOURCE/A,DEST/A,ALTDEST/S,COPY/S,DELETE/S,DELETEDIRS/S,NOCASE/S,NODEST/S,NOERRORS/S,NOHIDDEN/S,NOPROTECT/S,NORECURSE/S";

volatile bool g_bBreak;		/* Set to true if when ctrl-c is hit by the user */
RArgs g_oArgs;				/* Contains the parsed command line arguments */

/* Written: Friday 02-Jan-2009 10:30 am */

static void SignalHandler(int /*a_iSignal*/)
{
	/* Indicate that ctrl-c has been hit so that the scanning routine drops out */

	g_bBreak = true;
}

int main(int a_iArgC, const char *a_ppcArgV[])
{
	char *Source, *Dest;
	int Result;
	RScanner Scanner;

	/* Install a ctrl-c handler so we can handle ctrl-c being pressed and shut down the scan */
	/* properly */

	signal(SIGINT, SignalHandler);

	/* Parse the command line parameters passed in and make sure they are formatted correctly */

	if ((Result = g_oArgs.Open(g_accTemplate, ARGS_NUM_ARGS, a_ppcArgV, a_iArgC)) == KErrNone)
	{
		if (g_oArgs.Valid() >= 2)
		{
			/* RScanner::Scan() is able to modify the parameters passed in so make a copy of the */
			/* source and destination paths before proceeding */

			Source = Scanner.QualifyFileName("", g_oArgs[ARGS_SOURCE]);
			Dest = Scanner.QualifyFileName("", g_oArgs[ARGS_DEST]);

			if ((Source) && (Dest))
			{
				Result = Scanner.Scan(Source, Dest);
			}
			else
			{
				Utils::Error("Out of memory");
			}

			delete Dest;
			delete Source;
		}
		else
		{
			Utils::Error("Required argument missing");
		}

		g_oArgs.Close();
	}
	else
	{
		// TODO: CAW - Obtain command line arguments on ? + check for Valid() isn't required on Amiga OS
		Utils::Error("Unable to read command line arguments");
	}

	return((Result == KErrNone) ? RETURN_OK : RETURN_ERROR);
}