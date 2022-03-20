#include <PalmOS.h>
#include <stdio.h>
#include <string.h>
#include "resID.h"
#include "wordlists.h"

#define WORD_LEN  5
#define MAX_GUESS 6
#define B26_LEN   2

#define TBL_X  12        // left
#define TBL_Y  25        // top
#define TBL_CW 20        // cell width
#define TBL_CH 17        // cell height
#define TBL_C  WORD_LEN  // columns
#define TBL_R  MAX_GUESS // rows
#define TBL_W (TBL_CW * TBL_C)  // total width
#define TBL_H (TBL_CH * TBL_R)  // total height

#define ALPHABET    "abcdefghijklmnopqrstuvwxyz"
#define ALPHA_LEN   26
#define ALPHA_ROWS  9
#define ALPHA_COLS  3
#define ALPHA_X    (TBL_X + TBL_W + 13)
#define ALPHA_Y     23
#define ALPHA_X_STP 10
#define ALPHA_Y_STP 12

#define GUESS_XSPC TBL_CW
#define GUESS_YSPC TBL_CH
#define GUESS_XOFF (GUESS_XSPC / 2)
#define GUESS_YOFF 2

typedef enum {
	enrWrongLetter = 0,
	enrWrongPosition,
	enrCorrectLetter
} GuessResult;

typedef enum {
	enNoGame = 0,
	enDailyGame,
	enUntrackedGame
} PalmdleState;

typedef struct t_PalmdleGame {
	UInt16 uiAnswerIndex;                     // index of the answer
	char szWord[WORD_LEN + 1];                // word to be guessed
	char szGuesses[MAX_GUESS][WORD_LEN + 1];  // stores the play field
	char szGuessedLetters[ALPHA_LEN];         // store which letters have already been guessed
	UInt8 ucGuessCount;                       // number of current guess (starts at 0)
	PalmdleState enState;                     // state of the game
} PalmdleGame;

typedef struct t_PalmdlePrefs {
	DateType dateLastPlayed;                  // date which the last daily game was played
	DateType dateLastWon;                     // date which the last daily game was won
	UInt16 uiStreak;                          // streak counter
	UInt16 uiMaxStreak;                       // longest streak
	UInt16 uiGamesWon;                        // total games won
	UInt16 uiGamesPlayed;                     // total games played
	PalmdleGame stSavedGame;                  // a saved game
} PalmdlePrefs;

typedef struct t_PalmdleVars {
	PalmdleGame stGame;                       // game variables
	MemPtr allowed_ptr;                       // dictionary list pointer
	MemHandle allowed_hdl;                    // and handle
	MemPtr allowed_idx_ptr;                   // dictionary index pointer
	MemHandle allowed_idx_hdl;                // and handle
	MemPtr answer_ptr;                        // answer list pointer
	MemHandle answer_hdl;                     // and handle
	Boolean boolHideLetters;                  // whether or not letters are to be hidden (share mode)
	char szTitle[32];                         // form title (has day number)
	PalmdlePrefs* pstPrefs;                   // pointer to loaded preferences
	Boolean fPrefsLoaded;                     // flag to only load prefs once
	FormType* frmMain;                        // main game form
} PalmdleVars;

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
 * Description: converts upper case char to lower case in place
 * Input      : c
 * Output     : c
 * Return     : none
 ***************************/
void cToLower(char* c) {
	if (*c >= (char)'A' && *c <= (char)'Z') *c += 32;
}

/***************************
 * Description: checks if char is uppercase
 * Input      : c
 * Output     : none
 * Return     : true if upper
 ***************************/
Boolean cIsUpper(char c) {
	if (c >= (char)'A' && c <= (char)'Z') return true;
	return false;
}

/***************************
 * Description: draws the play field guess table
 * Input      : none
 * Output     : none
 * Return     : none
 ***************************/
static void DrawGuessTable(void) {
	RectangleType pRect;
	RctSetRectangle(&pRect, TBL_X+1, TBL_Y+1, TBL_W-1, TBL_H-1);
	
	CustomPatternType blank;
	MemSet(&blank, sizeof(CustomPatternType), 0);
	WinSetPattern(&blank);
	WinFillRectangle(&pRect, 0);
	
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
 * Description: draws the guessed letters
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void DrawGuessedLetters(PalmdleVars* pstVars) {
	PalmdleGame* pstGame = &pstVars->stGame;
	unsigned int i;
	unsigned char x = 0, y = 0;
	char c;
	RectangleType pRect;

	CustomPatternType ptBlank;
	MemSet(&ptBlank, sizeof(CustomPatternType), 0);
	WinSetPattern(&ptBlank);

	for (i = 0; i < ALPHA_LEN; i++) {
		//y = i % ALPHA_ROWS;
		x = i % ALPHA_COLS;
		c = pstGame->szGuessedLetters[i];
		if (pstVars->boolHideLetters) c = ' ';
		FntSetFont(stdFont);
		if (c != ' ') {
			if (cIsUpper(c)) FntSetFont(boldFont);
			cToUpper(&c);
			WinDrawChars(&c, 1,
				ALPHA_X + (x * ALPHA_X_STP) - (FntCharWidth(c) / 2),
				ALPHA_Y + (y * ALPHA_Y_STP));
		} else {
			c = (char)'A' + i;
			FntSetFont(boldFont);
			RctSetRectangle(&pRect,
				ALPHA_X + (x * ALPHA_X_STP) - (FntCharWidth(c) / 2),
				ALPHA_Y + (y * ALPHA_Y_STP),
				FntCharWidth(c),
				FntCharHeight());
			WinFillRectangle(&pRect, 0);
		}
		//if (!((i + 1) % ALPHA_ROWS)) x += 1;
		if (!((i + 1) % ALPHA_COLS)) y += 1;
	}
}

/***************************
 * Description: determines the marker to be used for a letter of a guess
 * Input      : szGuess - the guess entered
 *            : szAnswer - the game's current answer to be guessed
 * Output     : penResult - array of 5 GuessResults
 *            : szGuessedLetters - modified to mark already guessed letters
 * Return     : none
 ***************************/
static void DetermineResult(GuessResult* penResult, const char* szGuess, const char* szAnswer, char* szGuessedLetters) {
	char szTempAnswer[WORD_LEN+1];
	MemMove(szTempAnswer, szAnswer, WORD_LEN+1);

	char c;
	int i, n;
	int iGuessIdx;
	for (i = 0; i < WORD_LEN; i++) {
		c = szGuess[i]; 
		cToUpper(&c);
		iGuessIdx = c - 'A';
		if (szTempAnswer[i] == c) {
			penResult[i] = enrCorrectLetter;
			szTempAnswer[i] = ' ';
			cToUpper(&szGuessedLetters[iGuessIdx]);
		}
	}

	for (i = 0; i < WORD_LEN; i++) {
		if(penResult[i] != enrWrongLetter) continue;
		c = szGuess[i];
		cToUpper(&c);
		iGuessIdx = c - 'A';
		for(n = 0; n < WORD_LEN; n++) {
			if(szTempAnswer[n] == ' ') continue;
			if (szTempAnswer[n] == c) {
				penResult[i] = enrWrongPosition;
				szTempAnswer[n] = ' ';
				cToUpper(&szGuessedLetters[iGuessIdx]);
			}
		}
		if (penResult[i] == enrWrongLetter && !cIsUpper(szGuessedLetters[iGuessIdx])) {
			szGuessedLetters[iGuessIdx] = ' ';
		}
	}
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
 * Description: draws a guess word across a row of the play field, along with markers
 * Input      : ucRow - which row to draw
 *            : pstVars - vars struct, new guess must already be populated
 * Output     : none
 * Return     : none
 ***************************/
static void DrawGuess(UInt8 ucRow, PalmdleVars* pstVars) {
	PalmdleGame* pstGame = &pstVars->stGame;
	if (!pstGame->szGuesses[ucRow][0]) return;

	int chr_x = TBL_X + GUESS_XOFF;
	int chr_y = TBL_Y + GUESS_YOFF + (ucRow * GUESS_YSPC);
	int i;
	char cCurChar;

	FntSetFont(largeFont);
	GuessResult enResult[WORD_LEN];
	MemSet(enResult, sizeof(enResult), 0);
	DetermineResult(enResult, pstGame->szGuesses[ucRow], pstGame->szWord, pstGame->szGuessedLetters);
	for (i = 0; i < TBL_C; i++) {
		cCurChar = pstGame->szGuesses[ucRow][i];
		if (cCurChar == 0) cCurChar = (char)' ';
		cToUpper(&cCurChar);
		if (!pstVars->boolHideLetters) {
			WinDrawChars(&cCurChar, 1, chr_x - (FntCharWidth(cCurChar) / 2), chr_y);
		}

		MarkSquare(i, ucRow, enResult[i]);
		chr_x += GUESS_XSPC;
	}
}

/***************************
 * Description: determines if an input is a valid palmdle guess
 * Input      : szGuess - string to check against
 *            : pstVars - vars struct
 * Output     : none
 * Return     : true or false
 ***************************/
Boolean IsValidGuess(const char* szGuess, PalmdleVars* pstVars) {
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
		uiGuessIndex += *(unsigned char*)(pstVars->allowed_idx_ptr + i);
	}

	for (i = 0; i < ANSWER_COUNT; i++) {
		if (!MemCmp(szUpperGuess, pstVars->answer_ptr + (i * WORD_LEN), WORD_LEN)) return true;
	}

	for (i = 0; i < *(unsigned char*)(pstVars->allowed_idx_ptr + uiGuessPrefix); i++) {
		if (!MemCmp(ucGuess, pstVars->allowed_ptr + ((i + uiGuessIndex) * B26_LEN), B26_LEN)) return true;
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
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameSubmitGuess(PalmdleVars* pstVars) {
	PalmdleGame* pstGame = &pstVars->stGame;
	if (pstGame->ucGuessCount < MAX_GUESS) {
		FieldType* objGuess = (FieldType*)FrmGetObjectPtr(pstVars->frmMain,
			FrmGetObjectIndex(pstVars->frmMain, FieldInput));
		char* szGuess = FldGetTextPtr(objGuess);

		if (IsValidGuess(szGuess, pstVars)) {
			// add guess to board
			MemMove(pstGame->szGuesses[pstGame->ucGuessCount], szGuess, WORD_LEN+1);
			DrawGuess(pstGame->ucGuessCount, pstVars);
			pstGame->ucGuessCount++;

			DrawGuessedLetters(pstVars);

			// check if game has been lost or won
			if (CaselessCompare(szGuess, pstGame->szWord, WORD_LEN)) {
				FrmAlert(AlertWin);
				pstGame->ucGuessCount = MAX_GUESS;

				if (pstGame->enState == enDailyGame) {
					DateType dateToday;
					DateSecondsToDate(TimGetSeconds(), &dateToday);

					pstVars->pstPrefs->uiStreak += 1;
					if (pstVars->pstPrefs->uiStreak > pstVars->pstPrefs->uiMaxStreak) {
						pstVars->pstPrefs->uiMaxStreak = pstVars->pstPrefs->uiStreak;
					}

					DateSecondsToDate(TimGetSeconds(), &pstVars->pstPrefs->dateLastWon);
					pstVars->pstPrefs->uiGamesWon += 1;
				}
				
				pstGame->enState = enNoGame;
			} else if (pstGame->ucGuessCount == MAX_GUESS) {
				FrmCustomAlert(AlertLose, pstGame->szWord, "", "");

				if (pstGame->enState == enDailyGame) {
					pstVars->pstPrefs->uiStreak = 0;
				}

				pstGame->enState = enNoGame;
			}

			// erase guess field
			MemHandle pTextH = FldGetTextHandle(objGuess);
			FldSetTextHandle(objGuess, NULL);
			MemSet(MemHandleLock(pTextH), WORD_LEN, 0);
			MemHandleUnlock(pTextH);
			FldSetTextHandle(objGuess, pTextH);
			FldDrawField(objGuess);
			FrmSetFocus(pstVars->frmMain, FrmGetObjectIndex(pstVars->frmMain, FieldInput));
		}
	}
}

/***************************
 * Description: load the game preferences
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void PrefsLoad(PalmdleVars* pstVars) {
	if (!PrefGetAppPreferencesV10(MainFtrCreator, AppVersion, pstVars->pstPrefs, sizeof(PalmdlePrefs))) {
		MemSet(pstVars->pstPrefs, sizeof(PalmdlePrefs), 0);
	}

	// 2.0 date fix?
	if (pstVars->pstPrefs->dateLastPlayed.year == 0) {
		DateSecondsToDate(TimGetSeconds(), &pstVars->pstPrefs->dateLastPlayed);
		DateAdjust(&pstVars->pstPrefs->dateLastPlayed, -1);
	}
}

/***************************
 * Description: save the game preferences
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void PrefsSave(PalmdleVars* pstVars, Boolean fSaveGame) {
	if (fSaveGame) {
		MemMove(&pstVars->pstPrefs->stSavedGame, &pstVars->stGame, sizeof(PalmdleGame));
	}
	PrefSetAppPreferencesV10(MainFtrCreator, AppVersion, pstVars->pstPrefs, sizeof(PalmdlePrefs));
}

/***************************
 * Description: initialize game struct
 * Input      : pstVars - vars struct, game state must be enNoGame for new game
 *            : fIsDaily - are we initing a daily game or a random game?
 * Output     : none
 * Return     : none
 ***************************/
static void GameInit(PalmdleVars* pstVars, Boolean fIsDaily) {
	PalmdleGame* pstGame = &pstVars->stGame;

	if (pstGame->enState != enNoGame) {
		if (FrmAlert(AlertForfeit)) return;
		MemSet(&pstVars->stGame, sizeof(PalmdleGame), 0);
	}

	if (!pstVars->fPrefsLoaded) {
		PrefsLoad(pstVars);
		pstVars->fPrefsLoaded = true;
	
		if (pstVars->pstPrefs->stSavedGame.enState != enNoGame) {
			// there's a game saved, restore it
			MemMove(pstGame, &pstVars->pstPrefs->stSavedGame, sizeof(PalmdleGame));
		}
	}

	DateType stDate;
	DateSecondsToDate(TimGetSeconds(), &stDate);

	if ((Int32)DateToDays(stDate) - (Int32)DateToDays(pstVars->pstPrefs->dateLastPlayed) < 1) {
		// already played this game
		if (fIsDaily && pstGame->enState == enNoGame) {
			FrmAlert(AlertAlreadyDaily);
			fIsDaily = false;
		}
	} else {
		// new daily available
		if (pstGame->enState != enNoGame) {
			// but we already restored a game
			switch (FrmAlert(AlertNewDaily)) {
				case 0:
					pstGame->enState = enNoGame;
					fIsDaily = true;
					break;
				default:
					break;
			}
		}
	}

	if (pstGame->enState == enNoGame) {
		// new game
		pstGame->ucGuessCount = 0;
		StrPrintF(pstGame->szGuessedLetters, ALPHABET);
		MemSet(pstGame->szGuesses, sizeof(pstGame->szGuesses), 0);

		if (fIsDaily) {
			if (DateToDays(stDate) - DateToDays(pstVars->pstPrefs->dateLastWon) > 1) {
				pstVars->pstPrefs->uiStreak = 0;
			}

			DateType stBaseDate;
			MemSet(&stBaseDate, sizeof(DateType), 0);
			stBaseDate.year = 2021 - 1904;
			stBaseDate.month = 6;
			stBaseDate.day = 19;

			UInt16 uiDateDifference = (DateToDays(stDate) - DateToDays(stBaseDate));
			pstGame->uiAnswerIndex = uiDateDifference % ANSWER_COUNT;

			pstGame->enState = enDailyGame;
			pstVars->pstPrefs->dateLastPlayed = stDate;
			pstVars->pstPrefs->uiGamesPlayed += 1;
		} else {
			// random index game
			pstGame->uiAnswerIndex = SysRandom(0) % ANSWER_COUNT;
			pstGame->enState = enUntrackedGame;
		}
	}

	MemMove(pstGame->szWord, pstVars->answer_ptr + pstGame->uiAnswerIndex * (WORD_LEN), WORD_LEN);
	pstGame->szWord[WORD_LEN] = (char)'\0';

	pstVars->boolHideLetters = false;

	if (pstGame->enState == enDailyGame) {
		StrPrintF(pstVars->szTitle, "%d", pstGame->uiAnswerIndex);
	} else {
		StrPrintF(pstVars->szTitle, "Random");
	}
}

/***************************
 * Description: redraw the current form and the entire game field
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameUpdateScreen(PalmdleVars* pstVars) {
	FrmEraseForm(pstVars->frmMain);
	FrmDrawForm(pstVars->frmMain);
	FntSetFont(stdFont);
	WinDrawChars(pstVars->szTitle, strlen(pstVars->szTitle), 48, 2);
	DrawGuessTable();
	
	int i;
	for (i = 0; i < MAX_GUESS; i++) {
		DrawGuess(i, pstVars);
	}

	DrawGuessedLetters(pstVars);

	FrmSetFocus(pstVars->frmMain, FrmGetObjectIndex(pstVars->frmMain, FieldInput));
}

/***************************
 * Description: update stats table
 * Input      : pTable - table pointer
 *            : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void UpdateStatsTable(TableType* objTable, PalmdleVars* pstVars) {
	char szTableLabels[4][16];
	int i;

	StrPrintF(szTableLabels[0], "Games Played");
	StrPrintF(szTableLabels[1], "Games Won");
	StrPrintF(szTableLabels[2], "Current Streak");
	StrPrintF(szTableLabels[3], "Longest Streak");

	for (i = 0; i < 4; i++) {
		TblSetItemStyle(objTable, i, 0, labelTableItem);
		TblSetItemStyle(objTable, i, 1, numericTableItem);
		TblSetItemPtr(objTable, i, 0, szTableLabels[i]);
		TblSetRowUsable(objTable, i, true);
		TblMarkRowInvalid(objTable, i);
	}

	TblSetItemInt(objTable, 0, 1, pstVars->pstPrefs->uiGamesPlayed);
	TblSetItemInt(objTable, 1, 1, pstVars->pstPrefs->uiGamesWon);
	TblSetItemInt(objTable, 2, 1, pstVars->pstPrefs->uiStreak);
	TblSetItemInt(objTable, 3, 1, pstVars->pstPrefs->uiMaxStreak);

	TblSetColumnUsable(objTable, 0, true);
	TblSetColumnUsable(objTable, 1, true);
	
	TblRedrawTable(objTable);
}

/***************************
 * Description: show stats form and handle
 * Input      : frmMain - the main form to return to
 *            : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void ShowStatsForm(FormType* frmMain, PalmdleVars* pstVars) {
	EventType event;
	UInt16 error;
	FormType* frmDialog = FrmInitForm(FormStats);
	FrmSetActiveForm(frmDialog);

	TableType* objTable = (TableType*)FrmGetObjectPtr(frmDialog,
			FrmGetObjectIndex(frmDialog, TableStats));
	FrmDrawForm(frmDialog);
	UpdateStatsTable(objTable, pstVars);

	do {
		EvtGetEvent(&event, evtWaitForever);
		
		if (SysHandleEvent(&event))
			continue;

		if (event.eType == ctlSelectEvent) {
			if (event.data.ctlSelect.controlID == ButtonDialog) {
				break;
			}

			if (event.data.ctlSelect.controlID == ButtonDelete) {
				if (!FrmAlert(AlertDelete)) {
					MemSet(pstVars->pstPrefs, sizeof(PalmdlePrefs), 0);
					PrefsSave(pstVars, false);
					UpdateStatsTable(objTable, pstVars);
				}
			}
		}

		FrmHandleEvent(frmDialog, &event);
	} while (event.eType != appStopEvent);

	FrmEraseForm(frmDialog);
	FrmSetActiveForm(frmMain);
	FrmDeleteForm(frmDialog);
}

/***************************
 * Description: show dialog form and handle
 * Input      : frmMain - the main form to return to
 *              uiFormID - ID of the (generic) dialog form to show
 * Output     : none
 * Return     : none
 ***************************/
static void ShowDialogForm(FormType* frmMain, UInt16 uiFormID) {
	EventType event;
	UInt16 error;
	FormType* frmDialog = FrmInitForm(uiFormID);
	FrmSetActiveForm(frmDialog);
	FrmDrawForm(frmDialog);
	do {
		EvtGetEvent(&event, evtWaitForever);
		
		if (SysHandleEvent(&event))
			continue;

		if (event.eType == ctlSelectEvent) {
			if (event.data.ctlSelect.controlID == ButtonDialog) {
				break;
			}
		}

		FrmHandleEvent(frmDialog, &event);
	} while (event.eType != appStopEvent);

	FrmEraseForm(frmDialog);
	FrmSetActiveForm(frmMain);
	FrmDeleteForm(frmDialog);
}

/***************************
 * Description: toggle whether letters are hidden for sharing
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void ToggleHideLetters(PalmdleVars* pstVars) {
	pstVars->boolHideLetters = !pstVars->boolHideLetters;
	GameUpdateScreen(pstVars);
}

/***************************
 * Description: event handler for main form
 * Input      : event - data to be handled
 *            : pstVars - vars struct
 * Output     : none
 * Return     : true if event was handled
 ***************************/
static Boolean GameHandleEvent(EventType* event, PalmdleVars* pstVars) {
	switch (event->eType) {
		case frmOpenEvent:
		case frmGotoEvent:
		case frmUpdateEvent:
			GameUpdateScreen(pstVars);
			return true;

		case keyDownEvent:
			if (event->data.keyDown.chr == (WChar)'\n') {
				GameSubmitGuess(pstVars);
				return true;
			}
			break;

		case ctlSelectEvent:
			if (event->data.ctlSelect.controlID == ButtonOK) {
				GameSubmitGuess(pstVars);
				return true;
			}
			break;

		case menuEvent:
			switch(event->data.menu.itemID) {
				case MenuAbout:
					ShowDialogForm(FrmGetActiveForm(), FormAbout);
					return true;

				case MenuHelp:
					ShowDialogForm(FrmGetActiveForm(), FormHelp);
					return true;

				case MenuStats:
					ShowStatsForm(FrmGetActiveForm(), pstVars);
					return true;

				case MenuNewGame:
					GameInit(pstVars, false);
					GameUpdateScreen(pstVars);
					return true;
					
				case MenuHideLetters:
					ToggleHideLetters(pstVars);
					return true;
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

		PalmdleVars* pstVars = (PalmdleVars*)MemPtrNew(sizeof(PalmdleVars));
		if ((UInt32)pstVars == 0) return -1;
		MemSet(pstVars, sizeof(PalmdleVars), 0);
		pstVars->frmMain = frmMain;
		
		pstVars->pstPrefs = (PalmdlePrefs*)MemPtrNew(sizeof(PalmdlePrefs));
		if ((UInt32)pstVars->pstPrefs == 0) return -1;
		MemSet(pstVars->pstPrefs, sizeof(PalmdlePrefs), 0);

		pstVars->allowed_hdl = DmGet1Resource(0x776f7264, AllowedListFile);
		pstVars->allowed_ptr = MemHandleLock(pstVars->allowed_hdl);
		if((UInt32)pstVars->allowed_ptr == 0) return -1;

		pstVars->allowed_idx_hdl = DmGet1Resource(0x776f7264, AllowedIndexFile);
		pstVars->allowed_idx_ptr = MemHandleLock(pstVars->allowed_idx_hdl);
		if((UInt32)pstVars->allowed_idx_ptr == 0) return -1;

		pstVars->answer_hdl = DmGet1Resource(0x776f7264, AnswerListFile);
		pstVars->answer_ptr = MemHandleLock(pstVars->answer_hdl);
		if((UInt32)pstVars->answer_ptr == 0) return -1;
		
		GameInit(pstVars, true);
		GameUpdateScreen(pstVars);

		do {
			EvtGetEvent(&event, evtWaitForever);
			
			if (GameHandleEvent(&event, pstVars))
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

		PrefsSave(pstVars, true);

		MemHandleUnlock(pstVars->allowed_hdl);
		DmReleaseResource(pstVars->allowed_hdl);
		MemHandleUnlock(pstVars->allowed_idx_hdl);
		DmReleaseResource(pstVars->allowed_idx_hdl);
		MemHandleUnlock(pstVars->answer_hdl);
		DmReleaseResource(pstVars->answer_hdl);
		MemPtrFree((MemPtr)pstVars->pstPrefs);
		MemPtrFree((MemPtr)pstVars);
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