#include <PalmOS.h>
#include <stdio.h>
#include <string.h>
#include "resID.h"
#include "wordlists.h"
#include "strings.h"

#define WORD_LEN  5
#define MAX_GUESS 6
#define B26_LEN   2

#define TBL_X  10        // left
#define TBL_Y  25        // top
#define TBL_CW 20        // cell width
#define TBL_CH 17        // cell height
#define TBL_C  WORD_LEN  // columns
#define TBL_R  MAX_GUESS // rows
#define TBL_W (TBL_CW * TBL_C)  // total width
#define TBL_H (TBL_CH * TBL_R)  // total height

#define ALPHABET       "abcdefghijklmnopqrstuvwxyz"
#define ALPHA_LEN       26
#define ALPHA_ROWS      9
#define ALPHA_COLS      3
#define ALPHA_X        (TBL_X + TBL_W + 15)
#define ALPHA_Y         23
#define ALPHA_X_STEP    10
#define ALPHA_Y_STEP    12
#define ALPHA_X_CENTER (TBL_X + TBL_W + 25)

#define GUESS_XSPC TBL_CW
#define GUESS_YSPC TBL_CH
#define GUESS_XOFF (GUESS_XSPC / 2)
#define GUESS_YOFF 2

#define COLOR_YELLOW 115
#define COLOR_GREEN  211

typedef enum {
	enrWrongLetter = 0,
	enrWrongPosition,
	enrCorrectLetter
} GuessResult;

typedef enum {
	enNoGame = 0,
	enDailyGame,      // "classic", uses sequential daily answer
	enUntrackedGame,  // "random", doesn't affect stats
	enCheckedGame,    // "online", uses NYT ID
} PalmdleState;

// -- kept for 1.2 prefs compat

typedef struct t_PalmdleGameV1 {
	UInt16 uiAnswerIndex;                     // index of the answer
	char szWord[WORD_LEN + 1];                // word to be guessed
	char szGuesses[MAX_GUESS][WORD_LEN + 1];  // stores the play field
	char szGuessedLetters[ALPHA_LEN];         // store which letters have already been guessed
	UInt8 ucGuessCount;                       // number of current guess (starts at 0)
	PalmdleState enState;                     // state of the game
} PalmdleGameV1;

typedef struct t_PalmdlePrefsV1 {
	DateType dateLastPlayed;                  // date which the last daily game was played
	DateType dateLastWon;                     // date which the last daily game was won
	UInt16 uiStreak;                          // streak counter
	UInt16 uiMaxStreak;                       // longest streak
	UInt16 uiGamesWon;                        // total games won
	UInt16 uiGamesPlayed;                     // total games played
	PalmdleGameV1 stSavedGame;                  // a saved game
} PalmdlePrefsV1;

// --

typedef struct t_PalmdleGame {
	// Palmdle up to 1.2 - AppVersionV1
	UInt16 uiAnswerIndex;                     // index of the answer (game number)
	char szWord[WORD_LEN + 1];                // word to be guessed
	char szGuesses[MAX_GUESS][WORD_LEN + 1];  // stores the play field
	char szGuessedLetters[ALPHA_LEN];         // store which letters have already been guessed
	UInt8 ucGuessCount;                       // number of current guess (starts at 0)
	PalmdleState enState;                     // state of the game
	// new to Palmdle 2.0
	UInt16 uiCheckedIndex;                    // NYT given ID (new answer index)
} PalmdleGame;

typedef struct t_PalmdlePrefs {
	// Palmdle up to 1.2 - AppVersionV1
	DateType dateLastPlayed;                  // date which the last daily game was played
	DateType dateLastWon;                     // date which the last daily game was won
	UInt16 uiStreak;                          // streak counter
	UInt16 uiMaxStreak;                       // longest streak
	UInt16 uiGamesWon;                        // total games won
	UInt16 uiGamesPlayed;                     // total games played
	PalmdleGame stSavedGame;                  // a saved game
	// new to Palmdle 2.0
	Boolean boolReadAbout;                    // don't show about at launch
	Boolean boolAlwaysClassic;                // don't ask for daily ID
	UInt16 uiGuessWon[MAX_GUESS];             // count of games won per guesses taken
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
	UInt8 ucDisplayDepth;                     // detected display depth
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
 * Description: empties the area with the guessed letters
 * Input      : none
 * Output     : none
 * Return     : none
 ***************************/
static void EmptyGuessedLetters() {
	RectangleType pRect;

	CustomPatternType ptBlank;
	MemSet(&ptBlank, sizeof(CustomPatternType), 0);
	WinSetPattern(&ptBlank);

	RctSetRectangle(&pRect,
		ALPHA_X,
		ALPHA_Y,
		ALPHA_X + (ALPHA_COLS * ALPHA_X_STEP),
		ALPHA_Y + (ALPHA_ROWS * ALPHA_Y_STEP));
	WinFillRectangle(&pRect, 0);
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
		FntSetFont(stdFont);
		if (c != ' ') {
			if (cIsUpper(c)) FntSetFont(boldFont);
			cToUpper(&c);
			WinDrawChars(&c, 1,
				ALPHA_X + (x * ALPHA_X_STEP) - (FntCharWidth(c) / 2),
				ALPHA_Y + (y * ALPHA_Y_STEP));
		} else {
			c = (char)'A' + i;
			FntSetFont(boldFont);
			RctSetRectangle(&pRect,
				ALPHA_X + (x * ALPHA_X_STEP) - (FntCharWidth(c) / 2),
				ALPHA_Y + (y * ALPHA_Y_STEP),
				FntCharWidth(c),
				FntCharHeight());
			WinFillRectangle(&pRect, 0);
		}
		//if (!((i + 1) % ALPHA_ROWS)) x += 1;
		if (!((i + 1) % ALPHA_COLS)) y += 1;
	}
}

/***************************
 * Description: draws the stats in place of the guessed letters
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void DrawStatsBoard(PalmdleVars* pstVars) {
	char szLabels[4][10];
	char szVals[4][8];

	FntSetFont(boldFont);
	int x = 0;
	int y = ALPHA_Y + 4;

	StrPrintF(szLabels[0], STATS_SMALL_PLAYED);
	StrPrintF(szVals[0], "%d", pstVars->pstPrefs->uiGamesPlayed);
	StrPrintF(szLabels[1], STATS_SMALL_WON);
	StrPrintF(szVals[1], "%d", pstVars->pstPrefs->uiGamesWon);
	StrPrintF(szLabels[2], STATS_SMALL_STREAK);
	StrPrintF(szVals[2], "%d", pstVars->pstPrefs->uiStreak);
	StrPrintF(szLabels[3], STATS_SMALL_MAX_STREAK);
	StrPrintF(szVals[3], "%d", pstVars->pstPrefs->uiMaxStreak);

	for (int i = 0; i < 4; i++) {
		FntSetFont(boldFont);
		x = ALPHA_X_CENTER - (FntLineWidth(szLabels[i], StrLen(szLabels[i])) / 2);
		WinDrawChars(szLabels[i], StrLen(szLabels[i]),
			x,
			y);
		y += FntLineHeight();

		FntSetFont(stdFont);
		x = ALPHA_X_CENTER - (FntLineWidth(szVals[i], StrLen(szVals[i])) / 2);
		WinDrawChars(szVals[i], StrLen(szVals[i]),
			x,
			y);
		y += FntLineHeight() + 4;
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
static void MarkSquare(UInt8 ucColumn, UInt8 ucRow, GuessResult enResult, Boolean fColor) {
	RectangleType pRect;
	RctSetRectangle(&pRect,
		TBL_X + (ucColumn * TBL_CW) + 3,
		TBL_Y + (ucRow * TBL_CH) + 3,
		TBL_CW - 5,
		TBL_CH - 5);

	if (fColor) {
		WinPushDrawState();
		WinSetDrawMode(winPaint);
	}

	switch (enResult) {
		case enrWrongLetter:
			break;

		case enrWrongPosition:
			if (fColor) {
				WinSetBackColor(COLOR_YELLOW); // yellow
				WinPaintRectangleFrame(boldRoundFrame, &pRect);
			} else {
				WinDrawGrayRectangleFrame(boldRoundFrame, &pRect);
			}
			break;
		
		case enrCorrectLetter:
			if (fColor) {
				WinSetBackColor(COLOR_GREEN); // yellow
				WinPaintRectangleFrame(boldRoundFrame, &pRect);
			} else {
				WinDrawRectangleFrame(boldRoundFrame, &pRect);
			}
			break;
	}

	if (fColor) {
		WinPopDrawState();
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

		MarkSquare(i, ucRow, enResult[i], (pstVars->ucDisplayDepth >= 8));
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
 * Description: checks if game state should have stats tracked
 * Input      : enState - game state
 * Output     : none
 * Return     : true or false
 ***************************/
static Boolean IsTrackedGame(PalmdleState enState) {
	if (enState == enDailyGame) return true;
	if (enState == enCheckedGame) return true;
	return false;
}

/***************************
 * Description: uses form guess field to process and populate next guess in game
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void GameSubmitGuess(PalmdleVars* pstVars) {
	PalmdleGame* pstGame = &pstVars->stGame;

	if (pstVars->boolHideLetters) return;

	if (pstGame->ucGuessCount < MAX_GUESS) {
		FieldType* objGuess = (FieldType*)FrmGetObjectPtr(pstVars->frmMain,
			FrmGetObjectIndex(pstVars->frmMain, FieldInput));
		char* szGuess = FldGetTextPtr(objGuess);
		if (szGuess == NULL || strlen(szGuess) < 5) return;

		if (IsValidGuess(szGuess, pstVars)) {
			// add guess to board
			MemMove(pstGame->szGuesses[pstGame->ucGuessCount], szGuess, WORD_LEN+1);
			DrawGuess(pstGame->ucGuessCount, pstVars);
			pstGame->ucGuessCount++;

			DrawGuessedLetters(pstVars);

			// check if game has been lost or won
			if (CaselessCompare(szGuess, pstGame->szWord, WORD_LEN)) {
				FrmAlert(AlertWin);
				pstVars->pstPrefs->uiGuessWon[pstGame->ucGuessCount-1] += 1;
				pstGame->ucGuessCount = MAX_GUESS;

				if (IsTrackedGame(pstGame->enState)) {
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

				if (IsTrackedGame(pstGame->enState)) {
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
	MemSet(pstVars->pstPrefs, sizeof(PalmdlePrefs), 0);

	// check for saved prefs for earlier version
	if (PrefGetAppPreferencesV10(MainFtrCreator, AppVersionV1, pstVars->pstPrefs, sizeof(PalmdlePrefsV1))) {
		// do nothing, loaded old prefs
	} else {
		PrefGetAppPreferencesV10(MainFtrCreator, AppVersion, pstVars->pstPrefs, sizeof(PalmdlePrefs));
		// either loaded new prefs or didn't, prefs already initialized to 0 anyway
	}

	// Palm OS 2.0 date fix?
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
 * Description: check if answer ID is valid and warn if not
 * Input      : uiCheckID - answer ID to check
 *              szCheckID - string version of ID
 * Output     : none
 * Return     : true if valid, false otherwise
 ***************************/
static Boolean CheckValidAnswerID(UInt16 uiCheckID, const char* szCheckID) {
	Boolean boolValid = true;
	char szTmp[maxStrIToALen];

	StrIToA(szTmp, uiCheckID);
	if (StrCompare(szTmp, szCheckID)) boolValid = false;

	// NYT starts id at 1 not 0
	// v2.1: we are now pulling from allowed guesses as well
	if (uiCheckID - 1 >= ANSWER_COUNT + ALLOWED_COUNT) boolValid = false;

	if (!boolValid) {
		FrmAlert(AlertInvalidID);
		return false;
	}

	return true;
}

/***************************
 * Description: show ID input form and handle
 * Input      : frmMain - the main form to return to
 *            : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void ShowIDInputForm(FormType* frmMain, PalmdleVars* pstVars) {
	EventType event;
	UInt16 error;
	FormType* frmDialog = FrmInitForm(FormIDInput);
	FrmSetActiveForm(frmDialog);
	FrmDrawForm(frmDialog);

	char szTmp[32];
	StrPrintF(szTmp, FORM_ID_PROMPT, pstVars->stGame.uiAnswerIndex);
	FntSetFont(boldFont);
	WinDrawChars(szTmp, strlen(szTmp), 10, 89);

	UInt16 uiFieldIndex = FrmGetObjectIndex(frmDialog, FieldID);
	FrmSetFocus(frmDialog, uiFieldIndex);

	// preemptively set random game in case form is closed an unhandled way
	pstVars->stGame.enState = enUntrackedGame;

	do {
		EvtGetEvent(&event, evtWaitForever);
		
		if (SysHandleEvent(&event))
			continue;

		if (event.eType == ctlSelectEvent) {
			if (event.data.ctlSelect.controlID == ButtonDialog) {
				// play online game, check entered id is valid and set up online game
				FieldType* objIDField = (FieldType*)FrmGetObjectPtr(frmDialog,
					uiFieldIndex);
				char* szCheckID = FldGetTextPtr(objIDField);
				if (szCheckID) {
					UInt16 uiCheckID = StrAToI(szCheckID);
					
					if (CheckValidAnswerID(uiCheckID, szCheckID)) {
						pstVars->stGame.enState = enCheckedGame;
						pstVars->stGame.uiCheckedIndex = uiCheckID - 1; // NYT starts id at 1 not 0
						break;
					}
				}
			}

			if (event.data.ctlSelect.controlID == ButtonOffline) {
				// play classic game, answer index is same as game number
				pstVars->stGame.enState = enDailyGame;
				pstVars->stGame.uiCheckedIndex = pstVars->stGame.uiAnswerIndex;
				break;
			}

			if (event.data.ctlSelect.controlID == ButtonRandom) {
				// play random game, don't affect stats
				// we already set this at the start so quitting the form is safe
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
 * Description: fetch word from allowed guesses list and populate solution word
 * Input      : pstVars - vars struct, game state must be enNoGame for new game
 * Output     : none
 * Return     : none
 ***************************/
static void FetchSolutionFromAllowed(PalmdleVars* pstVars) {
	PalmdleGame* pstGame = &pstVars->stGame;

	unsigned int uiWantedIndex = pstGame->uiCheckedIndex - ANSWER_COUNT;

	unsigned int uiAllowedIndex = 0;
	unsigned int uiAllowedCount = 0;
	unsigned int i;
	for (i = 0; i < ALLOWED_INDEX_COUNT; i++) {
		uiAllowedCount += *(unsigned char*)(pstVars->allowed_idx_ptr + i);
		if (uiAllowedCount >= uiWantedIndex) {
			uiAllowedIndex = i;
			break;
		}
	}

	unsigned char ucAllowedWord[B26_LEN];
	MemMove(ucAllowedWord, pstVars->allowed_ptr + (uiWantedIndex * (B26_LEN)), B26_LEN);
	unsigned int uiAllowedWordInt = ucAllowedWord[0] << 8 | ucAllowedWord[1];

	pstGame->szWord[0] = 'A' + (uiAllowedIndex / 26);
	pstGame->szWord[1] = 'A' + (uiAllowedIndex % 26);
	pstGame->szWord[4] = 'A' + (uiAllowedWordInt % 26);
	uiAllowedWordInt /= 26;
	pstGame->szWord[3] = 'A' + (uiAllowedWordInt % 26);
	uiAllowedWordInt /= 26;
	pstGame->szWord[2] = 'A' + (uiAllowedWordInt % 26);
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

			if (pstGame->uiAnswerIndex > 505) {
				ShowIDInputForm(pstVars->frmMain, pstVars);
			}
			
			// user decided to play random game instead. goto is easiest
			if (pstGame->enState == enUntrackedGame) goto setRandomGame;

			pstVars->pstPrefs->dateLastPlayed = stDate;
			pstVars->pstPrefs->uiGamesPlayed += 1;
		} else {
			// random index game
setRandomGame:
			pstGame->uiCheckedIndex = SysRandom(0) % ANSWER_COUNT;
			pstGame->enState = enUntrackedGame;
		}
	}

	if (pstGame->uiCheckedIndex < ANSWER_COUNT) {
		MemMove(pstGame->szWord, pstVars->answer_ptr + pstGame->uiCheckedIndex * (WORD_LEN), WORD_LEN);
	} else {
		FetchSolutionFromAllowed(pstVars);
	}
	pstGame->szWord[WORD_LEN] = (char)'\0';

	pstVars->boolHideLetters = false;

	switch (pstGame->enState) {
		case enDailyGame:
			StrPrintF(pstVars->szTitle, "%d (%s)", pstGame->uiAnswerIndex, OFFLINE_TITLE);
			break;
		case enCheckedGame:
			StrPrintF(pstVars->szTitle, "%d (%s)", pstGame->uiAnswerIndex, ONLINE_TITLE);
			break;
		default:
			StrPrintF(pstVars->szTitle, RANDOM_TITLE);
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

	if (pstVars->boolHideLetters) {
		DrawStatsBoard(pstVars);
	} else {
		DrawGuessedLetters(pstVars);
	}

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

	StrPrintF(szTableLabels[0], STATS_PLAYED);
	StrPrintF(szTableLabels[1], STATS_WON);
	StrPrintF(szTableLabels[2], STATS_STREAK);
	StrPrintF(szTableLabels[3], STATS_MAX_STREAK);

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
 *              pstVars - Palmdle vars
 * Output     : none
 * Return     : none
 ***************************/
static void ShowDialogForm(FormType* frmMain, UInt16 uiFormID, PalmdleVars* pstVars) {
	EventType event;
	UInt16 error;
	FormType* frmDialog = FrmInitForm(uiFormID);
	FrmSetActiveForm(frmDialog);
	FrmDrawForm(frmDialog);

	ControlPtr ctl = NULL;
	if (uiFormID == FormAbout) {
		UInt16 obj = FrmGetObjectIndex(frmDialog, CheckboxRead);
		if (obj != frmInvalidObjectId) {
			ctl = (ControlPtr)FrmGetObjectPtr(frmDialog, obj);
			CtlSetValue (ctl, pstVars->pstPrefs->boolReadAbout);
		}
	}

	do {
		EvtGetEvent(&event, evtWaitForever);
		
		if (SysHandleEvent(&event))
			continue;

		if (event.eType == ctlSelectEvent) {
			if (event.data.ctlSelect.controlID == ButtonDialog) {
				break;
			}

			if (event.data.ctlSelect.controlID == CheckboxRead) {
				if (ctl != NULL) {
					pstVars->pstPrefs->boolReadAbout = CtlGetValue(ctl);
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
 * Description: toggle whether letters are hidden for sharing
 * Input      : pstVars - vars struct
 * Output     : none
 * Return     : none
 ***************************/
static void ToggleHideLetters(PalmdleVars* pstVars) {
	pstVars->boolHideLetters = !pstVars->boolHideLetters;
	//EmptyGuessedLetters();
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
					ShowDialogForm(FrmGetActiveForm(), FormAbout, pstVars);
					return true;

				case MenuHelp:
					ShowDialogForm(FrmGetActiveForm(), FormHelp, pstVars);
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
		
		// store device screen depth
		UInt32 ulDisplayDepth = 0;
		FtrGet(sysFtrCreator, sysFtrNumDisplayDepth, &ulDisplayDepth);
		pstVars->ucDisplayDepth = (UInt8)ulDisplayDepth;

		GameInit(pstVars, true);
		GameUpdateScreen(pstVars);

		if (!pstVars->pstPrefs->boolReadAbout) {
			ShowDialogForm(FrmGetActiveForm(), FormAbout, pstVars);
		}

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