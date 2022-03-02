#include <PalmOS.h>
#include <stdio.h>
#include <string.h>
#include "resID.h"
#include "wordlist.h"
#include "allowedlist.h"

#define WORD_LEN 5
#define MAX_GUESS 6
#define B26_LEN 3

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

typedef struct t_WordleGame {
	char szWord[WORD_LEN + 1];
	char szGuesses[MAX_GUESS][WORD_LEN + 1];
	UInt8 ucGuessCount;
	MemPtr allowed_ptr;
	MemPtr answer_ptr;
	Boolean boolHideLetters;
} WordleGame;

// makes the table outline for the guess letters
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

// checks whether a letter in one guess is correct
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

// draws guess results markers on table
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

void cToUpper(char* c) {
	if (*c >= (char)'a' && *c <= (char)'z') *c -= 32;
}

// prints the guess letters to the table
static void DrawGuess(UInt8 ucRow, WordleGame* pstGame) {
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

// determines if an input is a valid wordle guess
Boolean IsValidGuess(const char* szGuess, WordleGame* pstGame) {
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
		ulB26 = ulB26 * 26;
		ulB26 += (int)(c - (char)'A');
	}

	unsigned char ucGuess[] = {
		(unsigned char)(ulB26>>16)&0xFF,
		(unsigned char)(ulB26>>8)&0xFF,
		(unsigned char)(ulB26)&0xFF
	};

	for (i = 0; i < ANSWER_COUNT; i++) {
		if (!MemCmp(szUpperGuess, pstGame->answer_ptr + (i * WORD_LEN), WORD_LEN)) return true;
	}

	for (i = 0; i < allowed_count; i++) {
		if (!MemCmp(ucGuess, pstGame->allowed_ptr + (i * B26_LEN), B26_LEN)) return true;
	}
	
	return false;
}

// caseless string compare, true if same, false if not
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

// called when OK is pressed on guess field
static void GameSubmitGuess(WordleGame* pstGame) {
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
		}
	}
}

// initializes the game struct
static void GameInit(WordleGame* pstGame) {
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
}

// refreshes the form screen
static void GameUpdateScreen(WordleGame* pstGame) {
	FrmDrawForm(FrmGetActiveForm());
	DrawGuessTable();
	
	int i;
	for (i = 0; i < MAX_GUESS; i++) {
		DrawGuess(i, pstGame);
	}
}

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

static void ToggleHideLetters(WordleGame* pstGame) {
	pstGame->boolHideLetters = !pstGame->boolHideLetters;
	GameUpdateScreen(pstGame);
}

// event handler
static Boolean GameHandleEvent(EventType* event, WordleGame* pstGame) {
	switch (event->eType) {
		case frmLoadEvent:
			GameInit(pstGame);
			return true;

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
		WordleGame* pstGame = (WordleGame*)MemPtrNew(sizeof(WordleGame));
		if ((UInt32)pstGame == 0) return -1;
		MemSet(pstGame, sizeof(WordleGame), 0);

		pstGame->allowed_ptr = MemPtrNew(allowed_count * 3);
		if((UInt32)pstGame->allowed_ptr == 0) return -1;
		MemMove(pstGame->allowed_ptr, allowed_list, allowed_count * 3);

		pstGame->answer_ptr = MemPtrNew(ANSWER_COUNT * WORD_LEN);
		if((UInt32)pstGame->answer_ptr == 0) return -1;
		MemMove(pstGame->answer_ptr, ANSWER_LIST, ANSWER_COUNT * WORD_LEN);
		
		GameInit(pstGame);

		FormType* frmMain = FrmInitForm(FormMain);
		FrmSetActiveForm(frmMain);
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

		MemPtrFree(pstGame->allowed_ptr);
		MemPtrFree(pstGame->answer_ptr);
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