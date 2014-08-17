// Lexer for SQL, including PL/SQL and SQL*Plus.

#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <string>
#include <vector>
#include <map>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "CharacterSet.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "LexerModule.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

static inline bool IsSqlWordChar(int ch, bool sqlAllowDottedWord) {
	if (!sqlAllowDottedWord)
		return (ch < 0x80) && (IsAlphaNumeric(ch) || ch == '_');
	else
		return (ch < 0x80) && (IsAlphaNumeric(ch) || ch == '_' || ch == '.');
}

static inline  bool IsANumberChar(int ch, int chPrev) {
	return (ch < 0x80) && (IsADigit(ch) || (ch == '.' && chPrev != '.')
		|| ((ch == '+' || ch == '-') && (chPrev == 'e' || chPrev == 'E'))
		|| (ch == 'e' || ch == 'E') && (chPrev < 0x80) && IsADigit(chPrev));
}

/*static const char * const sqlWordListDesc[] = {
	"Keywords",
	"Database Objects",
	"PLDoc",
	"SQL*Plus",
	"User Keywords 1",
	"User Keywords 2",
	"User Keywords 3",
	"User Keywords 4",
	0
};*/

static void ColouriseSqlDoc(unsigned int startPos, int length, int initStyle, WordList *keywordlists[], Accessor &styler) {
	WordList &keywords1 = *keywordlists[0];
	WordList &keywords2 = *keywordlists[1];
	WordList &kw_user1 = *keywordlists[2];
	//WordList &kw_pldoc = *keywordlists[2];
	//WordList &kw_sqlplus = *keywordlists[3];
	//WordList &kw_user2 = *keywordlists[5];
	//WordList &kw_user3 = *keywordlists[6];
	//WordList &kw_user4 = *keywordlists[7];

	const bool sqlBackticksIdentifier = styler.GetPropertyInt("lexer.sql.backticks.identifier", 1) != 0;
	const bool sqlNumbersignComment = styler.GetPropertyInt("lexer.sql.numbersign.comment", 1) != 0;
	const bool sqlBackslashEscapes = styler.GetPropertyInt("lexer.sql.backslash.escapes", 1) != 0;
	const bool sqlAllowDottedWord = styler.GetPropertyInt("lexer.sql.allow.dotted.word", 0) != 0;

	StyleContext sc(startPos, length, initStyle, styler);
	//int styleBeforeDCKeyword = SCE_SQL_DEFAULT;
	int offset = 0;
	for (; sc.More(); sc.Forward(), offset++) {
		// Determine if the current state should terminate.
		switch (sc.state) {
		case SCE_SQL_OPERATOR:
			sc.SetState(SCE_SQL_DEFAULT);
			break;
		case SCE_SQL_HEX:
			if (!IsHexDigit(sc.ch)) {
				sc.SetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_HEX2:
			if (sc.ch == '\"' || sc.ch == '\'') {
				sc.ForwardSetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_BIT:
			if (!(sc.ch == '0' || sc.ch == '1')) {
				sc.SetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_BIT2:
			if (sc.ch == '\'') {
				sc.ForwardSetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_NUMBER:
			// We stop the number definition on non-numerical non-dot non-eE non-sign char
			if (!IsANumberChar(sc.ch, sc.chPrev)) {
				sc.SetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_VARIABLE:
			if (!(iswordchar(sc.ch) || sc.ch == '@')) {
				sc.SetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_IDENTIFIER:
_label_identifier:
			if (!IsSqlWordChar(sc.ch, sqlAllowDottedWord)) {
				int nextState = SCE_SQL_DEFAULT;
				char s[128];
				sc.GetCurrentLowered(s, sizeof(s));
				if (keywords1.InList(s)) {
					sc.ChangeState(SCE_SQL_WORD);
				} else if (keywords2.InList(s)) {
					sc.ChangeState(SCE_SQL_WORD2);
				} else if (LexGetNextChar(sc.currentPos, styler) == '(' && kw_user1.InListAbbreviated(s, '(')) {
					sc.ChangeState(SCE_SQL_USER1);
				}
				//} else if (kw_sqlplus.InListAbbreviated(s, '~')) {
				//	sc.ChangeState(SCE_SQL_SQLPLUS);
				//	if (strncmp(s, "rem", 3) == 0) {
				//		nextState = SCE_SQL_SQLPLUS_COMMENT;
				//	} else if (strncmp(s, "pro", 3) == 0) {
				//		nextState = SCE_SQL_SQLPLUS_PROMPT;
				//	}
				//} else if (kw_user2.InList(s)) {
				//	sc.ChangeState(SCE_SQL_USER2);
				//} else if (kw_user3.InList(s)) {
				//	sc.ChangeState(SCE_SQL_USER3);
				//} else if (kw_user4.InList(s)) {
				//	sc.ChangeState(SCE_SQL_USER4);
				//}
				sc.SetState(nextState);
			}
			break;
		case SCE_SQL_QUOTEDIDENTIFIER:
			if (sc.ch == 0x60) {
				if (sc.chNext == 0x60) {
					sc.Forward();	// Ignore it
				} else {
					sc.ForwardSetState(SCE_SQL_DEFAULT);
				}
			}
			break;
		case SCE_SQL_COMMENT:
			if (sc.Match('*', '/')) {
				sc.Forward();
				sc.ForwardSetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_COMMENTLINE:
		case SCE_SQL_COMMENTLINEDOC:
		//case SCE_SQL_SQLPLUS_COMMENT:
		//case SCE_SQL_SQLPLUS_PROMPT:
			if (sc.atLineStart) {
				sc.SetState(SCE_SQL_DEFAULT);
			}
			break;
		case SCE_SQL_CHARACTER:
			if (sqlBackslashEscapes && sc.ch == '\\') {
				sc.Forward();
			} else if (sc.ch == '\'') {
				if (sc.chNext == '\"') {
					sc.Forward();
				} else {
					sc.ForwardSetState(SCE_SQL_DEFAULT);
				}
			}
			break;
		case SCE_SQL_STRING:
			if (sc.ch == '\\') {
				// Escape sequence
				sc.Forward();
			} else if (sc.ch == '\"') {
				if (sc.chNext == '\"') {
					sc.Forward();
				} else {
					sc.ForwardSetState(SCE_SQL_DEFAULT);
				}
			}
			break;
		}

		// Determine if a new state should be entered.
		if (sc.state == SCE_SQL_DEFAULT) {
			if (sc.ch == '0' && (sc.chNext == 'x' || sc.chNext == 'X')) {
				sc.SetState(SCE_SQL_HEX);
				sc.Forward();
			} else if ((sc.ch == 'x' || sc.ch == 'X') && (sc.chNext == '\"' || sc.chNext == '\'')) {
				sc.SetState(SCE_SQL_HEX2);
				sc.Forward();
			} else if (sc.ch == '0' && (sc.chNext == 'b' || sc.chNext == 'B')) {
				sc.SetState(SCE_SQL_BIT);
				sc.Forward();
			} else if ((sc.ch == 'b' || sc.ch == 'B') && (sc.chNext == '\'')) {
				sc.SetState(SCE_SQL_BIT2);
				sc.Forward();
			} else if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_SQL_NUMBER);
			} else if ((sc.ch == '@' && iswordstart(sc.chNext))) {
				sc.SetState(SCE_SQL_VARIABLE);
			} else if (iswordstart(sc.ch)) {
				sc.SetState(SCE_SQL_IDENTIFIER);
			} else if (sc.ch == 0x60 && sqlBackticksIdentifier) {
				sc.SetState(SCE_SQL_QUOTEDIDENTIFIER);
			} else if (sc.Match('/', '*')) {
				sc.SetState(SCE_SQL_COMMENT);
				sc.Forward();	// Eat the * so it isn't used for the end of the comment
			} else if (sc.Match('-', '-')) {
				// MySQL requires a space or control char after --
				// http://dev.mysql.com/doc/mysql/en/ansi-diff-comments.html
				// Perhaps we should enforce that with proper property:
				//~} else if (sc.Match("-- ")) {
				sc.SetState(SCE_SQL_COMMENTLINE);
			} else if (sc.ch == '#' && sqlNumbersignComment) {
				sc.SetState(SCE_SQL_COMMENTLINEDOC);
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_SQL_CHARACTER);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_SQL_STRING);
			} else if (isoperator(static_cast<char>(sc.ch))) {
				sc.SetState(SCE_SQL_OPERATOR);
			}
		}
	}

	if (sc.state == SCE_SQL_IDENTIFIER)
		goto _label_identifier;

	sc.Complete();
}

enum {
	MASK_NESTED_CASES = 0x01FF,
	MASK_INTO_SELECT_STATEMENT_OR_ASSIGNEMENT = 0x0200,
	MASK_CASE_MERGE_WITHOUT_WHEN_FOUND = 0x0400,
	MASK_MERGE_STATEMENT = 0x0800,
	MASK_INTO_DECLARE = 0x1000,
	MASK_INTO_EXCEPTION = 0x2000,
	MASK_INTO_CONDITION = 0x4000,
	MASK_IGNORE_WHEN = 0x8000
};

class SQLStates {
public :
	void Set(int lineNumber, unsigned short int sqlStatesLine) {
		if (!sqlStatement.size() == 0 || !sqlStatesLine == 0) {
			sqlStatement.resize(lineNumber + 1, 0);
			sqlStatement[lineNumber] = sqlStatesLine;
		}
	}

	unsigned short int ForLine(int lineNumber) {
		if ((lineNumber > 0) && (sqlStatement.size() > static_cast<size_t>(lineNumber))) {
			return sqlStatement[lineNumber];
		} else {
			return 0;
		}
	}

	static unsigned short int IgnoreWhen (unsigned short int sqlStatesLine, bool enable) {
		if (enable)
			sqlStatesLine |= MASK_IGNORE_WHEN;
		else
			sqlStatesLine &= ~MASK_IGNORE_WHEN;

		return sqlStatesLine;
	}

	static unsigned short int IntoCondition (unsigned short int sqlStatesLine, bool enable) {
		if (enable)
			sqlStatesLine |= MASK_INTO_CONDITION;
		else
			sqlStatesLine &= ~MASK_INTO_CONDITION;

		return sqlStatesLine;
	}

	static unsigned short int IntoExceptionBlock (unsigned short int sqlStatesLine, bool enable) {
		if (enable)
			sqlStatesLine |= MASK_INTO_EXCEPTION;
		else
			sqlStatesLine &= ~MASK_INTO_EXCEPTION;

		return sqlStatesLine;
	}

	static unsigned short int IntoDeclareBlock (unsigned short int sqlStatesLine, bool enable) {
		if (enable)
			sqlStatesLine |= MASK_INTO_DECLARE;
		else
			sqlStatesLine &= ~MASK_INTO_DECLARE;

		return sqlStatesLine;
	}

	unsigned short int IntoMergeStatement (unsigned short int sqlStatesLine, bool enable) {
		if (enable)
			sqlStatesLine |= MASK_MERGE_STATEMENT;
		else
			sqlStatesLine &= ~MASK_MERGE_STATEMENT;

		return sqlStatesLine;
	}

	unsigned short int CaseMergeWithoutWhenFound (unsigned short int sqlStatesLine, bool found) {
		if (found)
			sqlStatesLine |= MASK_CASE_MERGE_WITHOUT_WHEN_FOUND;
		else
			sqlStatesLine &= ~MASK_CASE_MERGE_WITHOUT_WHEN_FOUND;

		return sqlStatesLine;
	}

	unsigned short int IntoSelectStatementOrAssignment (unsigned short int sqlStatesLine, bool found) {
		if (found)
			sqlStatesLine |= MASK_INTO_SELECT_STATEMENT_OR_ASSIGNEMENT;
		else
			sqlStatesLine &= ~MASK_INTO_SELECT_STATEMENT_OR_ASSIGNEMENT;

		return sqlStatesLine;
	}

	static unsigned short int BeginCaseBlock (unsigned short int sqlStatesLine) {
		if ((sqlStatesLine & MASK_NESTED_CASES) < MASK_NESTED_CASES) {
			sqlStatesLine++;
		}
		return sqlStatesLine;
	}

	static unsigned short int EndCaseBlock (unsigned short int sqlStatesLine) {
		if ((sqlStatesLine & MASK_NESTED_CASES) > 0) {
			sqlStatesLine--;
		}
		return sqlStatesLine;
	}

	static inline bool IsIgnoreWhen (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_IGNORE_WHEN) != 0;
	}

	static inline bool IsIntoCondition (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_INTO_CONDITION) != 0;
	}

	static inline bool IsIntoCaseBlock (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_NESTED_CASES) != 0;
	}

	static inline bool IsIntoExceptionBlock (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_INTO_EXCEPTION) != 0;
	}

	static inline bool IsIntoDeclareBlock (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_INTO_DECLARE) != 0;
	}

	bool IsIntoSelectStatementOrAssignment (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_INTO_SELECT_STATEMENT_OR_ASSIGNEMENT) != 0;
	}

	static bool IsCaseMergeWithoutWhenFound (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_CASE_MERGE_WITHOUT_WHEN_FOUND) != 0;
	}

	static bool IsIntoMergeStatement (unsigned short int sqlStatesLine) {
		return (sqlStatesLine & MASK_MERGE_STATEMENT) != 0;
	}

	SQLStates() {}

private :
	std::vector <unsigned short int> sqlStatement;
};

static inline bool IsStreamCommentStyle(int style) {
	return style == SCE_SQL_COMMENT;
}

static inline bool IsCommentStyle (int style) {
	return style == SCE_SQL_COMMENT || style == SCE_SQL_COMMENTLINE || style == SCE_SQL_COMMENTLINEDOC;
}

#define IsCommentLine(line)			IsLexCommentLine(line, styler, MultiStyle(SCE_SQL_COMMENTLINE, SCE_SQL_COMMENTLINEDOC))

static void FoldSqlDoc(unsigned int startPos, int length, int initStyle, WordList *[], Accessor &styler) {
	if (styler.GetPropertyInt("fold") == 0)
		return;
	const bool foldOnlyBegin = styler.GetPropertyInt("fold.sql.only.begin", 0) != 0;
	const bool foldComment = styler.GetPropertyInt("fold.comment", 1) != 0;
	const bool foldAtElse = styler.GetPropertyInt("fold.sql.at.else", 0) != 0;
	const bool foldCompact = styler.GetPropertyInt("fold.compact", 0) != 0;

	SQLStates sqlStates;
	unsigned int endPos = startPos + length;
	int visibleChars = 0;
	int lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0) {
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
	}
	int levelNext = levelCurrent;

	char ch, chNext;
	int stylePrev, style, styleNext;
	chNext = styler[startPos];
	chNext = styler[startPos];
	style = initStyle;
	styleNext = styler.StyleAt(startPos);
	bool atEOL;
	bool endFound = false;
	bool isUnfoldingIgnored = false;
	// this statementFound flag avoids to fold when the statement is on only one line by ignoring ELSE or ELSIF
	// eg. "IF condition1 THEN ... ELSIF condition2 THEN ... ELSE ... END IF;"
	bool statementFound = false;
	unsigned short int sqlStatesCurrentLine = 0;
	if (foldOnlyBegin) {
		sqlStatesCurrentLine = sqlStates.ForLine(lineCurrent);
	}

	for (unsigned int i = startPos; i < endPos; i++) {
		ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (atEOL || (!IsCommentStyle(style) && ch == ';')) {
			if (endFound) {
				//Maybe this is the end of "EXCEPTION" BLOCK (eg. "BEGIN ... EXCEPTION ... END;")
				sqlStatesCurrentLine = SQLStates::IntoExceptionBlock(sqlStatesCurrentLine, false);
			}
			// set endFound and isUnfoldingIgnored to false if EOL is reached or ';' is found
			endFound = false;
			isUnfoldingIgnored = false;
		}
		if ((!IsCommentStyle(style) && ch == ';')) {
			if (sqlStates.IsIntoMergeStatement(sqlStatesCurrentLine)) {
				// This is the end of "MERGE" statement.
				if (!sqlStates.IsCaseMergeWithoutWhenFound(sqlStatesCurrentLine))
					levelNext--;
				sqlStatesCurrentLine = sqlStates.IntoMergeStatement(sqlStatesCurrentLine, false);
				//sqlStatesCurrentLine = sqlStates.WhenThenFound(sqlStatesCurrentLine, false);
				levelNext--;
			}
			if (sqlStates.IsIntoSelectStatementOrAssignment(sqlStatesCurrentLine))
				sqlStatesCurrentLine = sqlStates.IntoSelectStatementOrAssignment(sqlStatesCurrentLine, false);
		}
		if (ch == ':' && chNext == '=' && !IsCommentStyle(style))
			sqlStatesCurrentLine = sqlStates.IntoSelectStatementOrAssignment(sqlStatesCurrentLine, true);

		if (foldComment && IsStreamCommentStyle(style)) {
			if (!IsStreamCommentStyle(stylePrev)) {
				levelNext++;
			} else if (!IsStreamCommentStyle(styleNext) && !atEOL) {
				// Comments don't end at end of line and the next character may be unstyled.
				levelNext--;
			}
		}
		// Disable explicit folding; it can often cause problems with non-aware code
		// MySQL needs -- comments to be followed by space or control char
		if (foldComment && atEOL && IsCommentLine(lineCurrent)) {
			if (!IsCommentLine(lineCurrent - 1) && IsCommentLine(lineCurrent + 1))
				levelNext++;
			else if (IsCommentLine(lineCurrent - 1) && !IsCommentLine(lineCurrent+1))
				levelNext--;
		}
		if (style == SCE_SQL_OPERATOR) {
			if (ch == '(') {
				if (levelCurrent > levelNext)
					levelCurrent--;
				levelNext++;
			} else if (ch == ')') {
				levelNext--;
			} else if ((foldOnlyBegin) && ch == ';') {
				sqlStatesCurrentLine = SQLStates::IgnoreWhen(sqlStatesCurrentLine, false);
			}
		}
		// If new keyword (cannot trigger on elseif or nullif, does less tests)
		if (style == SCE_SQL_WORD && stylePrev != SCE_SQL_WORD) {
			const int MAX_KW_LEN = 9;	// Maximum length of folding keywords
			char s[MAX_KW_LEN + 2];
			unsigned int j = 0;
			for (; j < MAX_KW_LEN + 1; j++) {
				if (!iswordchar(styler[i + j])) {
					break;
				}
				s[j] = static_cast<char>(tolower(styler[i + j]));
			}
			if (j == MAX_KW_LEN + 1) {
				// Keyword too long, don't test it
				s[0] = '\0';
			} else {
				s[j] = '\0';
			}

			if (!foldOnlyBegin && strcmp(s, "select") == 0) {
				sqlStatesCurrentLine = sqlStates.IntoSelectStatementOrAssignment(sqlStatesCurrentLine, true);
			} else if (strcmp(s, "if") == 0) {
				if (endFound) {
					endFound = false;
					if (foldOnlyBegin && !isUnfoldingIgnored) {
						// this end isn't for begin block, but for if block ("end if;")
						// so ignore previous "end" by increment levelNext.
						levelNext++;
					}
				} else {
					if (!foldOnlyBegin)
						sqlStatesCurrentLine = SQLStates::IntoCondition(sqlStatesCurrentLine, true);
					if (levelCurrent > levelNext) {
						// doesn't include this line into the folding block
						// because doesn't hide IF (eg "END; IF")
						levelCurrent = levelNext;
					}
				}
			} else if (!foldOnlyBegin && strcmp(s, "then") == 0 &&
				SQLStates::IsIntoCondition(sqlStatesCurrentLine)) {
				sqlStatesCurrentLine = SQLStates::IntoCondition(sqlStatesCurrentLine, false);
				if (!foldOnlyBegin) {
					if (levelCurrent > levelNext) {
						levelCurrent = levelNext;
					}
					if (!statementFound)
						levelNext++;
					statementFound = true;
				} else if (levelCurrent > levelNext) {
					// doesn't include this line into the folding block
					// because doesn't hide LOOP or CASE (eg "END; LOOP" or "END; CASE")
					levelCurrent = levelNext;
				}
			} else if (strcmp(s, "loop") == 0 || strcmp(s, "case") == 0 || strcmp(s, "while") == 0 || strcmp(s, "repeat") == 0) {
				if (endFound) {
					endFound = false;
					if (foldOnlyBegin && !isUnfoldingIgnored) {
						// this end isn't for begin block, but for loop block ("end loop;") or case block ("end case;")
						// so ignore previous "end" by increment levelNext.
						levelNext++;
					}
					if ((!foldOnlyBegin) && strcmp(s, "case") == 0) {
						sqlStatesCurrentLine = SQLStates::EndCaseBlock(sqlStatesCurrentLine);
						if (!sqlStates.IsCaseMergeWithoutWhenFound(sqlStatesCurrentLine))
							levelNext--; //again for the "end case;" and block when
					}
				} else if (!foldOnlyBegin) {
					if (strcmp(s, "case") == 0) {
						sqlStatesCurrentLine = SQLStates::BeginCaseBlock(sqlStatesCurrentLine);
						sqlStatesCurrentLine = sqlStates.CaseMergeWithoutWhenFound(sqlStatesCurrentLine, true);
					}

					if (levelCurrent > levelNext)
						levelCurrent = levelNext;

					if (!statementFound)
						levelNext++;

					statementFound = true;
				} else if (levelCurrent > levelNext) {
					// doesn't include this line into the folding block
					// because doesn't hide LOOP or CASE (eg "END; LOOP" or "END; CASE")
					levelCurrent = levelNext;
				}
			} else if ((!foldOnlyBegin) && (foldAtElse && !statementFound) && strcmp(s, "elsif") == 0) {
				// folding for ELSE and ELSIF block only if foldAtElse is set
				// and IF or CASE aren't on only one line with ELSE or ELSIF (with flag statementFound)
				sqlStatesCurrentLine = SQLStates::IntoCondition(sqlStatesCurrentLine, true);
				levelCurrent--;
				levelNext--;
			} else if ((!foldOnlyBegin) && (foldAtElse && !statementFound) && strcmp(s, "else") == 0) {
				// folding for ELSE and ELSIF block only if foldAtElse is set
				// and IF or CASE aren't on only one line with ELSE or ELSIF (with flag statementFound)
				// prevent also ELSE is on the same line (eg. "ELSE ... END IF;")
				statementFound = true;
				if (sqlStates.IsIntoCaseBlock(sqlStatesCurrentLine) && sqlStates.IsCaseMergeWithoutWhenFound(sqlStatesCurrentLine)) {
					sqlStatesCurrentLine = sqlStates.CaseMergeWithoutWhenFound(sqlStatesCurrentLine, false);
					levelNext++;
				} else {
					// we are in same case "} ELSE {" in C language
					levelCurrent--;
				}
			} else if (strcmp(s, "begin") == 0 || strcmp(s, "start") == 0) {
				levelNext++;
				sqlStatesCurrentLine = SQLStates::IntoDeclareBlock(sqlStatesCurrentLine, false);
			} else if ((strcmp(s, "end") == 0) || (strcmp(s, "endif") == 0)) {
				// SQL Anywhere permits IF ... ELSE ... ENDIF
				// will only be active if "endif" appears in the
				// keyword list.
				endFound = true;
				levelNext--;
				if (sqlStates.IsIntoSelectStatementOrAssignment(sqlStatesCurrentLine) && !sqlStates.IsCaseMergeWithoutWhenFound(sqlStatesCurrentLine))
					levelNext--;
				if (levelNext < SC_FOLDLEVELBASE) {
					levelNext = SC_FOLDLEVELBASE;
					isUnfoldingIgnored = true;
				}
			} else if ((!foldOnlyBegin) && strcmp(s, "when") == 0 &&
				!SQLStates::IsIgnoreWhen(sqlStatesCurrentLine) &&
						!sqlStates.IsIntoExceptionBlock(sqlStatesCurrentLine) && (
							sqlStates.IsIntoCaseBlock(sqlStatesCurrentLine) ||
							sqlStates.IsIntoMergeStatement(sqlStatesCurrentLine)
							)
						) {
				sqlStatesCurrentLine = SQLStates::IntoCondition(sqlStatesCurrentLine, true);
				// Don't foldind when CASE and WHEN are on the same line (with flag statementFound) (eg. "CASE selector WHEN expression1 THEN sequence_of_statements1;\n")
				// and same way for MERGE statement.
				if (!statementFound) {
					if (!sqlStates.IsCaseMergeWithoutWhenFound(sqlStatesCurrentLine)) {
						levelCurrent--;
						levelNext--;
					}
					sqlStatesCurrentLine = sqlStates.CaseMergeWithoutWhenFound(sqlStatesCurrentLine, false);
				}
			} else if ((!foldOnlyBegin) && strcmp(s, "exit") == 0) {
				sqlStatesCurrentLine = SQLStates::IgnoreWhen(sqlStatesCurrentLine, true);
			} else if ((!foldOnlyBegin) && !SQLStates::IsIntoDeclareBlock(sqlStatesCurrentLine) && strcmp(s, "exception") == 0) {
				sqlStatesCurrentLine = SQLStates::IntoExceptionBlock(sqlStatesCurrentLine, true);
			} else if ((!foldOnlyBegin) &&
				(strcmp(s, "declare") == 0 || strcmp(s, "function") == 0 ||
				strcmp(s, "procedure") == 0 || strcmp(s, "package") == 0)) {
				sqlStatesCurrentLine = SQLStates::IntoDeclareBlock(sqlStatesCurrentLine, true);
			} else if ((!foldOnlyBegin) && strcmp(s, "merge") == 0) {
				sqlStatesCurrentLine = sqlStates.IntoMergeStatement(sqlStatesCurrentLine, true);
				sqlStatesCurrentLine = sqlStates.CaseMergeWithoutWhenFound(sqlStatesCurrentLine, true);
				levelNext++;
				statementFound = true;
			}
		}
		if (!isspacechar(ch)) {
			visibleChars++;
		}
		if (atEOL || (i == endPos-1)) {
			int levelUse = levelCurrent;
			int lev = levelUse | levelNext << 16;
			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelCurrent = levelNext;
			visibleChars = 0;
			statementFound = false;
			if (!foldOnlyBegin)
				sqlStates.Set(lineCurrent, sqlStatesCurrentLine);
		}
	}
}

LexerModule lmSQL(SCLEX_SQL, ColouriseSqlDoc, "sql", FoldSqlDoc);
