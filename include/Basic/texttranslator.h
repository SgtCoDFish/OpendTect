#ifndef texttranslator_h
#define texttranslator_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2010
 RCS:		$Id$
________________________________________________________________________

-*/


#include "basicmod.h"
#include "callback.h"
#include "uistring.h"
#include "threadlock.h"
#include "objectset.h"
#include "bufstring.h"
#include "bufstringset.h"

mFDQtclass(QTranslator);
mFDQtclass(QLocale);
mFDQtclass(QString);

class TranslatorLanguageInfo;


mExpClass(Basic) TextTranslateMgr : public CallBacker
{ mODTextTranslationClass(TextTranslateMgr);
public:
				TextTranslateMgr();
				~TextTranslateMgr();

    int				nrSupportedLanguages() const;
    uiString			getLanguageUserName(int) const;
    BufferString		getLanguageName(int) const;
    bool			setLanguage(int,uiString& errmsg);
    int				currentLanguage() const;

    Notifier<TextTranslateMgr>	languageChange;

    int				dirtyCount() const	{ return dirtycount_; }
				//Increased every time language is changed

    const mQtclass(QTranslator)* getQTranslator(const char* appl) const;
    const mQtclass(QLocale)*	getQLocale() const;

    static BufferString		getLocalizationName(const char* appl,
						    const char* lang);
    static char			cApplicationEnd() { return '_'; }

protected:

    void			loadInfo();

    int				dirtycount_;
    ObjectSet<TranslatorLanguageInfo>	languages_;
    int				currentlanguageidx_;
};


mGlobal(Basic) TextTranslateMgr& TrMgr();

#endif

