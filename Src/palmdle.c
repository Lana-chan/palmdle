#include <PalmOS.h>
#include <stdio.h>
#include <string.h>
#include "resID.h"
#include "wordlists.h"

#define WORD_LEN 5
#define MAX_GUESS 6
#define B26_LEN 2

#define TBL_X 30        // left
#define TBL_Y 25        // top
#define TBL_CW 20       // cell width
#define TBL_CH 17       // cell height
#define TBL_C WORD_LEN  // columns
#define TBL_R MAX_GUESS // rows
#define TBL_W (TBL_CW * TBL_C)  // total width
#define TBL_H (TBL_CH * TBL_R)  // total height

#define GUESS_XSPC TBL_CW
#define GUESS_YSPC TBL_CH
#define GUESS_XOFF (GUESS_XSPC / 2)
#define GUESS_YOFF 2

typedef enum {
	enrWrongLetter = 0,
	enrWrongPosition,
	enrCorrectLetter
} GuessResult;

typedef struct t_PalmdleGame {
	char szWord[WORD_LEN + 1];                // word to be guessed
	char szGuesses[MAX_GUESS][WORD_LEN + 1];  // stores the play field
	UInt8 ucGuessCount;                       // number of current guess (starts at 0)
	MemPtr allowed_ptr;                       // dictionary list pointer
	MemHandle allowed_hdl;                    // and handle
	MemPtr allowed_idx_ptr;                   // dictionary index pointer
	MemHandle allowed_idx_hdl;                // and handle
	MemPtr answer_ptr;                        // answer list pointer
	MemHandle answer_hdl;                     // and handle
	Boolean boolHideLetters;                  // whether or not letters are to be hidden (share mode)
	char szTitle[32];                         // form title (has day number)
} PalmdleGame;

/***************************
 * Description: draws the play field with guess table, letters and markers
 * Input      : none
 * Output     : none
 * Return     : none
 ***************************/
static void DrawGuessTable(void) {
	RectangleType pRect;
	RctSetRectangle(&pRect, TBL_X, TBL_Y, TBL_W, TBL_H);
	WinPushDrawState();
	CustomPatternType blank = {0};
	WinSetPattern(&blank);
	WinFillRectangle(&pRect, 0);
	WinPopDrawState();
	WinDrawRectangleFrame(simpleFrame, &pRect);
	int i, n;
	for (i = 1; i < TBL_C; i++) {
		n = TBL_X + (TBL_CW * i);
		WinDrawLine(n, TBL_Y, n, TBL_Y + TBL_H);
	}

	for (i = 1; i < TBL_R; i++) {
		n = TBL_Y + (TBL_CH * i);
		WinDrawLine(TBL_X, n, TBL_X + TBL_W, n);
	}
}

/***************************
 * Description: determines the marker to be used for a letter of a guess
 * Input      : cLetter - one letter of the result
 *            : iPosition - 0-idx of the letter
 *            : szAnswer - the game's current answer to be guessed
 * Output     : none
 * Return     : GuessResult
 ***************************/
static GuessResult DetermineResult(char cLetter, int iPosition, char* szAnswer) {
	if (szAnswer[iPosition] == cLetter) {
		szAnswer[iPosition] = (char)' ';
		return enrCorrectLetter;
	}

	int i;
	for (i = 0; i < WORD_LEN; i++) {
		if (i == iPosition) continue;
		if (szAnswer[i] == cLetter) {
			szAnswer[i] = (char)' ';
			return enrWrongPosition;
		}
	}

	return enrWrongLetter;
}

/***************************
 * Description: draws the guess marker on a letter cell
 * Input      : ucColumn, ucRow - 0-idx position of the marker
 *            : enResult - GuessResult to be drawn
 * Output     : none
 * Return     : none
 ***************************/
static void MarkSquare(UInt8 ucColumn, UInt8 ucRow, GuessResult enResult) {
	RectangleType pRect;
	RctSetRectangle(&pRect,
		TBL_X + (ucColumn * TBL_CW) + 3,
		TBL_Y + (ucRow * TBL_CH) + 3,
		TBL_CW - 5,
		TBL_CH - 5);

	switch (enResult) {
		case enrWrongLetter:
			return;

		case enrWrongPosition:
			WinDrawGrayRectangleFrame(boldRoundFrame, &pRect);
			break;
		
		case enrCorrectLetter:
			WinDrawRectangleFrame(boldRoundFrame, &pRect);
			break;
	}
}

/***************************
 * Description: converts lower case char to upper case in place
 * Input      : c
 * Output     : c
 * Return     : none
 ***************************/
void cToUpper(char* c) {
	if (*c >= (char)'a' && *c <= (char)'z') *c -= 32;
}

/***************************
 * Description: draws a guess word across a row of the play field
 * Input      : ucRow - which row to draw
 *            : pstGame - game struct, new guess must already be populated
 * Output     : none
 * Return     : none
 ***************************/
static void DrawGuess(UInt8 ucRow, PalmdleGame* pstGame) {
	if (!pstGame->szGuesses[ucRow][0]) return;

	int chr_x = TBL_X + GUESS_XOFF;
	int chr_y = TBL_Y + GUESS_YOFF + (ucRow * GUESS_YSPC);
	int i;
	char cCurChar;
	char szTempAnswer[WORD_LEN+1];
	MemMove(szTempAnswer, pstGame->szWord, WORD_LEN+1);

	FntSetFont(largeFont);
	for (i = 0; i < TBL_C; i++) {
		cCurChar = pstGame->szGuesses[ucRow][i];
		if (cCurChar == 0) cCurChar = (char)' ';
		cToUpper(&cCurChar);
		if (!pstGame->boolHideLetters) {
			WinDrawChars(&cCurChar, 1, chr_x - (FntCharWidth(cCurChar) / 2), chr_y);
		}

		GuessResult enResult = DetermineResult(cCurChar, i, szTempAnswer);
		MarkSquare(i, ucRow, enResult);
		chr_x += GUESS_XSPC;
	}
}

/***************************
 * Description: determines if an input is a valid palmdle guess
 * Input      : szGuess - string to check against
 *            : pstGame - game struct
 * Output     : none
 * Return     : true or false
 ***************************/
Boolean IsValidGuess(const char* szGuess, PalmdleGame* pstGame) {
	if (StrLen(szGuess) != WORD_LEN) return false;

	unsigned int i;
	char c;
	UInt32 ulB26 = 0;
	char szUpperGuess[5];
	for (i = 0; i < WORD_LEN; i++) {
		c = szGuess[i];
		cToUpper(&c);
		if (c < (char)'A' || c > (char)'Z') return false;
		szUpperGuess[i] = c;
		// to b26
		if (i > 1) {
			ulB26 = ulB26 * 26;
			ulB26 += (int)(c - (char)'A');
		}
	}
	unsigned int uiGuessPrefix = ((szUpperGuess[0] - (char)'A') * 26) + (szUpperGuess[1] - (char)'A');

	unsigned char ucGuess[] = {
		(unsigned char)(ulB26>>8)&0xFF,
		(unsigned char)(ulB26)&0xFF
	};

	unsigned int uiGuessIndex = 0;
	for (i = 0; i < uiGuessPrefix; i++) {
		uiGuessIndex += *(unsigned char*)(pstGame->allowed_idx_ptr + i);
	}

	for (i = 0; i < ANSWER_COUNT; i++) {
		if (!MemCmp(szUpperGuess, pstGame->answer_ptr + (i * WORD_LEN), WORD_LEN)) return true;
	}

	for (i = 0; i < *(unsigned char*)(pstGame->allowed_idx_ptr + uiGuessPrefix); i++) {
		if (!MemCmp(ucGuess, pstGame->allowed_ptr + ((i + uiGuessIndex) * B26_LEN), B26_LEN)) return true;
	}
	
	return false;
}

/***************************
 * Description: checks if two strings are the same, caseless
 * Input      : sz1, sz2 - strings to be compared
 *            : ucLen - length of chars to compare
 * Output     : none
 * Return     : true if same, false otherwise
 ***************************/
static Boolean CaselessCompare(const char* sz1, const char* sz2, UInt8 ucLen) {
	int i;
	char c1, c2;
	for (i = 0; i < ucLen; i++) {
		c1 = sz1[i];
		cToUpper(&c1);
		c2 = sz2[i];
		cToUpper(&c2);
		if (c1 != c2) return false;
	}
	
	return true;
}

/***************************
 * Description: uses form guess field to process and populate next guess in game
 * Input      : pstGame - game struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameSubmitGuess(PalmdleGame* pstGame) {
	if (pstGame->ucGuessCount < MAX_GUESS) {
		FieldType* objGuess = (FieldType*)FrmGetObjectPtr(FrmGetActiveForm(),
			FrmGetObjectIndex(FrmGetActiveForm(), FieldInput));
		char* szGuess = FldGetTextPtr(objGuess);

		if (IsValidGuess(szGuess, pstGame)) {
			// add guess to board
			MemMove(pstGame->szGuesses[pstGame->ucGuessCount], szGuess, WORD_LEN+1);
			DrawGuess(pstGame->ucGuessCount, pstGame);
			pstGame->ucGuessCount++;

			// check if game has been lost or won
			if (CaselessCompare(szGuess, pstGame->szWord, WORD_LEN)) {
				FrmAlert(AlertWin);
				pstGame->ucGuessCount = MAX_GUESS;
			} else if (pstGame->ucGuessCount == MAX_GUESS) {
				FrmCustomAlert(AlertLose, pstGame->szWord, "", "");
			}

			// erase guess field
			MemHandle pTextH = FldGetTextHandle(objGuess);
			FldSetTextHandle(objGuess, NULL);
			MemSet(MemHandleLock(pTextH), WORD_LEN, 0);
			MemHandleUnlock(pTextH);
			FldSetTextHandle(objGuess, pTextH);
			FldDrawField(objGuess);
			FldGrabFocus(objGuess);
		}
	}
}

/***************************
 * Description: initialize game struct
 * Input      : pstGame - game struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameInit(PalmdleGame* pstGame) {
	MemSet(pstGame->szGuesses, sizeof(pstGame->szGuesses), 0);

	DateType stDate;
	DateSecondsToDate(TimGetSeconds(), &stDate);

	DateType stBaseDate;
	MemSet(&stBaseDate, sizeof(DateType), 0);
	stBaseDate.year = 2021 - 1904;
	stBaseDate.month = 6;
	stBaseDate.day = 19;

	UInt16 uiDateDifference = (DateToDays(stDate) - DateToDays(stBaseDate));
	UInt16 uiAnswerIndex = uiDateDifference % ANSWER_COUNT;

	MemMove(pstGame->szWord, pstGame->answer_ptr + uiAnswerIndex * (WORD_LEN), WORD_LEN);
	pstGame->szWord[WORD_LEN] = (char)'\0';

	pstGame->ucGuessCount = 0;

	StrPrintF(pstGame->szTitle, "Palmdle %d\0", uiAnswerIndex);
	FrmSetTitle(FrmGetActiveForm(), pstGame->szTitle);
}

/***************************
 * Description: redraw the current form and the entire game field
 * Input      : pstGame - game struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameUpdateScreen(PalmdleGame* pstGame) {
	FrmDrawForm(FrmGetActiveForm());
	DrawGuessTable();
	
	int i;
	for (i = 0; i < MAX_GUESS; i++) {
		DrawGuess(i, pstGame);
	}
}

/***************************
 * Description: show about form and handle
 * Input      : frmMain - the main form to return to
 * Output     : none
 * Return     : none
 ***************************/
static void ShowAboutForm(FormType* frmMain) {
	EventType event;
	UInt16 error;
	FormType* frmAbout = FrmInitForm(FormAbout);
	FrmSetActiveForm(frmAbout);
	FrmDrawForm(frmAbout);
	do {
		EvtGetEvent(&event, evtWaitForever);
		
		if (SysHandleEvent(&event))
			continue;

		if (event.eType == ctlSelectEvent) {
			if (event.data.ctlSelect.controlID == ButtonAbout) {
				break;
			}
		}

		FrmHandleEvent(frmAbout, &event);
	} while (event.eType != appStopEvent);

	FrmEraseForm(frmAbout);
	FrmSetActiveForm(frmMain);
	FrmDeleteForm(frmAbout);
}

static void ToggleHideLetters(PalmdleGame* pstGame) {
	pstGame->boolHideLetters = !pstGame->boolHideLetters;
	GameUpdateScreen(pstGame);
}

// event handler
static Boolean GameHandleEvent(EventType* event, PalmdleGame* pstGame) {
	switch (event->eType) {
		case frmOpenEvent:
		case frmGotoEvent:
		case frmUpdateEvent:
			GameUpdateScreen(pstGame);
			return true;

		case keyDownEvent:
			if (event->data.keyDown.chr == (WChar)'\n') {
				GameSubmitGuess(pstGame);
				return true;
			}
			break;

		case ctlSelectEvent:
			if (event->data.ctlSelect.controlID == ButtonOK) {
				GameSubmitGuess(pstGame);
				return true;
			}
			break;

		case menuEvent:
			switch(event->data.menu.itemID) {
				case MenuAbout:
					ShowAboutForm(FrmGetActiveForm());
					return true;
					
				case MenuHideLetters:
					ToggleHideLetters(pstGame);
					break;
			}
			break;

		default:
			return false;
	}

	return false;
}

UInt32 PilotMain(UInt16 cmd, void *cmdPBP, UInt16 launchFlags) {
	EventType event;
	UInt16 error;

	if (sysAppLaunchCmdNormalLaunch == cmd) {
		FormType* frmMain = FrmInitForm(FormMain);
		FrmSetActiveForm(frmMain);

		PalmdleGame* pstGame = (PalmdleGame*)MemPtrNew(sizeof(PalmdleGame));
		if ((UInt32)pstGame == 0) return -1;
		MemSet(pstGame, sizeof(PalmdleGame), 0);

		pstGame->allowed_hdl = DmGet1Resource(0x776f7264, AllowedListFile);
		pstGame->allowed_ptr = MemHandleLock(pstGame->allowed_hdl);
		if((UInt32)pstGame->allowed_ptr == 0) return -1;

		pstGame->allowed_idx_hdl = DmGet1Resource(0x776f7264, AllowedIndexFile);
		pstGame->allowed_idx_ptr = MemHandleLock(pstGame->allowed_idx_hdl);
		if((UInt32)pstGame->allowed_idx_ptr == 0) return -1;

		pstGame->answer_hdl = DmGet1Resource(0x776f7264, AnswerListFile);
		pstGame->answer_ptr = MemHandleLock(pstGame->answer_hdl);
		if((UInt32)pstGame->answer_ptr == 0) return -1;
		
		GameInit(pstGame);
		GameUpdateScreen(pstGame);

		do {
			EvtGetEvent(&event, evtWaitForever);
			
			if (GameHandleEvent(&event, pstGame))
				continue;

			if (SysHandleEvent(&event))
				continue;

			if (MenuHandleEvent(NULL, &event, &error))
				continue;

			FrmHandleEvent(frmMain, &event);
		} while (event.eType != appStopEvent);

		FrmEraseForm(frmMain);
		FrmSetActiveForm(NULL);
		FrmDeleteForm(frmMain);

		MemHandleUnlock(pstGame->allowed_hdl);
		DmReleaseResource(pstGame->allowed_hdl);
		MemHandleUnlock(pstGame->allowed_idx_hdl);
		DmReleaseResource(pstGame->allowed_idx_hdl);
		MemHandleUnlock(pstGame->answer_hdl);
		DmReleaseResource(pstGame->answer_hdl);
		MemPtrFree((MemPtr)pstGame);
	}

	return 0;
}

UInt32 __attribute__((section(".vectors"))) __Startup__(void)
{
	SysAppInfoPtr appInfoP;
	void *prevGlobalsP;
	void *globalsP;
	UInt32 ret;
	
	SysAppStartup(&appInfoP, &prevGlobalsP, &globalsP);
	ret = PilotMain(appInfoP->cmd, appInfoP->cmdPBP, appInfoP->launchFlags);
	SysAppExit(appInfoP, prevGlobalsP, globalsP);
	
	return ret;
}