/*
 * Localized labels and descriptions for the 4-mode Door Skip selector
 * (Off, Classic, Fast, Supreme) in all 13 launcher languages.
 */
#pragma once

#include "launcher_i18n.h"

namespace cv_door_skip_text {

static const wchar_t* const k_labels[LANG_COUNT][4] = {
    /* pt-BR */ { L"Desligado", L"Cl\u00e1ssico", L"R\u00e1pido", L"Supremo" },
    /* en    */ { L"Off", L"Classic", L"Fast", L"Supreme" },
    /* es    */ { L"Desactivado", L"Cl\u00e1sico", L"R\u00e1pido", L"Supremo" },
    /* ja    */ { L"\u30aa\u30d5", L"\u30af\u30e9\u30b7\u30c3\u30af", L"\u9ad8\u901f", L"\u30b9\u30d7\u30ea\u30fc\u30e0" },
    /* fr    */ { L"D\u00e9sactiv\u00e9", L"Classique", L"Rapide", L"Supr\u00eame" },
    /* de    */ { L"Aus", L"Klassisch", L"Schnell", L"Supreme" },
    /* it    */ { L"Disattivato", L"Classico", L"Veloce", L"Supremo" },
    /* pl    */ { L"Wy\u0142\u0105czony", L"Klasyczny", L"Szybki", L"Supreme" },
    /* tr    */ { L"Kapal\u0131", L"Klasik", L"H\u0131zl\u0131", L"Supreme" },
    /* ar    */ { L"\u0625\u064a\u0642\u0627\u0641", L"\u0643\u0644\u0627\u0633\u064a\u0643\u064a", L"\u0633\u0631\u064a\u0639", L"\u0641\u0627\u0626\u0642" },
    /* zh    */ { L"\u5173\u95ed", L"\u7ecf\u5178", L"\u5feb\u901f", L"\u81f3\u5c0a" },
    /* ko    */ { L"\ub044\uae30", L"\ud074\ub798\uc2dd", L"\ube60\ub984", L"\uc288\ud504\ub9bc" },
    /* ru    */ { L"\u0412\u044b\u043a\u043b.", L"\u041a\u043b\u0430\u0441\u0441\u0438\u043a\u0430", L"\u0411\u044b\u0441\u0442\u0440\u044b\u0439", L"\u0421\u0443\u043f\u0440\u0438\u043c" },
};

static const wchar_t* const k_descs[LANG_COUNT][4] = {
    /* pt-BR */ {
        L"Anima\u00e7\u00e3o original das portas ativada.",
        L"Pula a anima\u00e7\u00e3o das portas com o carregamento padr\u00e3o da sala.",
        L"Carregamento acelerado das salas mantendo o fade suave.",
        L"Velocidade m\u00e1xima: troca de sala instant\u00e2nea, sem pausa de fade."
    },
    /* en */ {
        L"Original door opening animation plays normally.",
        L"Skips the door animation with standard room loading.",
        L"Accelerated room loading while keeping the smooth fade.",
        L"Maximum speed: instant room transition with no fade delay."
    },
    /* es */ {
        L"Se reproduce la animaci\u00f3n original al abrir puertas.",
        L"Salta la animaci\u00f3n de las puertas con carga est\u00e1ndar de la sala.",
        L"Carga acelerada de salas manteniendo el fundido suave.",
        L"Velocidad m\u00e1xima: transici\u00f3n instant\u00e1nea de sala, sin pausa de fundido."
    },
    /* ja */ {
        L"\u30aa\u30ea\u30b8\u30ca\u30eb\u306e\u6249\u958b\u9589\u30a2\u30cb\u30e1\u30fc\u30b7\u30e7\u30f3\u3092\u901a\u5e38\u901a\u308a\u518d\u751f\u3057\u307e\u3059\u3002",
        L"\u6249\u30a2\u30cb\u30e1\u30fc\u30b7\u30e7\u30f3\u3092\u30b9\u30ad\u30c3\u30d7\u3057\u3001\u6a19\u6e96\u901f\u5ea6\u3067\u6b21\u306e\u90e8\u5c4b\u3092\u8aad\u307f\u8fbc\u307f\u307e\u3059\u3002",
        L"\u6ed1\u3089\u304b\u306a\u30d5\u30a7\u30fc\u30c9\u3092\u4fdd\u3063\u305f\u307e\u307e\u3001\u90e8\u5c4b\u306e\u8aad\u307f\u8fbc\u307f\u3092\u9ad8\u901f\u5316\u3057\u307e\u3059\u3002",
        L"\u6700\u901f\u30e2\u30fc\u30c9\uff1a\u30d5\u30a7\u30fc\u30c9\u5f85\u3061\u306a\u3057\u3067\u5373\u5ea7\u306b\u6b21\u306e\u90e8\u5c4b\u3078\u5207\u308a\u66ff\u3048\u307e\u3059\u3002"
    },
    /* fr */ {
        L"L\u2019animation d\u2019ouverture des portes d\u2019origine est jou\u00e9e normalement.",
        L"Passe l\u2019animation des portes avec le chargement standard de la salle.",
        L"Chargement acc\u00e9l\u00e9r\u00e9 des salles tout en conservant le fondu doux.",
        L"Vitesse maximale : transition instantan\u00e9e sans d\u00e9lai de fondu."
    },
    /* de */ {
        L"Die originale T\u00fcranimation wird normal abgespielt.",
        L"\u00dcberspringt die T\u00fcranimation mit normalem Raum-Laden.",
        L"Beschleunigtes Laden der R\u00e4ume mit sanfter \u00dcberblendung.",
        L"Maximale Geschwindigkeit: sofortiger Raumwechsel ohne \u00dcberblendung."
    },
    /* it */ {
        L"Riproduce normalmente l\u2019animazione originale delle porte.",
        L"Salta l\u2019animazione delle porte con il caricamento standard della stanza.",
        L"Caricamento accelerato delle stanze mantenendo la dissolvenza fluida.",
        L"Velocit\u00e0 massima: cambio stanza istantaneo, senza pausa di dissolvenza."
    },
    /* pl */ {
        L"Oryginalna animacja otwierania drzwi odtwarza si\u0119 normalnie.",
        L"Pomija animacj\u0119 drzwi ze standardowym wczytywaniem pomieszczenia.",
        L"Przyspieszone wczytywanie pomieszcze\u0144 z zachowaniem p\u0142ynnego przej\u015bcia.",
        L"Maksymalna pr\u0119dko\u015b\u0107: natychmiastowa zmiana pomieszczenia bez wygaszania."
    },
    /* tr */ {
        L"Orijinal kap\u0131 a\u00e7\u0131lma animasyonu normal \u015fekilde oynat\u0131l\u0131r.",
        L"Kap\u0131 animasyonunu atlar ve oday\u0131 standart h\u0131zda y\u00fckler.",
        L"Yumu\u015fak kararmay\u0131 koruyarak oda y\u00fcklemesini h\u0131zland\u0131r\u0131r.",
        L"En y\u00fcksek h\u0131z: kararma gecikmesi olmadan an\u0131nda oda ge\u00e7i\u015fi."
    },
    /* ar */ {
        L"\u062a\u064f\u0639\u0631\u0636 \u062d\u0631\u0643\u0629 \u0641\u062a\u062d \u0627\u0644\u0623\u0628\u0648\u0627\u0628 \u0627\u0644\u0623\u0635\u0644\u064a\u0629 \u0628\u0634\u0643\u0644 \u0637\u0628\u064a\u0639\u064a.",
        L"\u064a\u062a\u062e\u0637\u0649 \u062d\u0631\u0643\u0629 \u0627\u0644\u0623\u0628\u0648\u0627\u0628 \u0645\u0639 \u0627\u0644\u062a\u062d\u0645\u064a\u0644 \u0627\u0644\u0642\u064a\u0627\u0633\u064a \u0644\u0644\u063a\u0631\u0641\u0629.",
        L"\u062a\u062d\u0645\u064a\u0644 \u0645\u064f\u0633\u0631\u0651\u0639 \u0644\u0644\u063a\u0631\u0641 \u0645\u0639 \u0627\u0644\u062d\u0641\u0627\u0638 \u0639\u0644\u0649 \u0627\u0644\u062a\u0644\u0627\u0634\u064a \u0627\u0644\u0633\u0644\u0633.",
        L"\u0623\u0642\u0635\u0649 \u0633\u0631\u0639\u0629: \u0627\u0646\u062a\u0642\u0627\u0644 \u0641\u0648\u0631\u064a \u0628\u064a\u0646 \u0627\u0644\u063a\u0631\u0641 \u0628\u062f\u0648\u0646 \u062a\u0623\u062e\u064a\u0631 \u0627\u0644\u062a\u0644\u0627\u0634\u064a."
    },
    /* zh-Hans */ {
        L"\u6b63\u5e38\u64ad\u653e\u539f\u7248\u5f00\u95e8\u52a8\u753b\u3002",
        L"\u8df3\u8fc7\u5f00\u95e8\u52a8\u753b\uff0c\u4f7f\u7528\u6807\u51c6\u623f\u95f4\u52a0\u8f7d\u901f\u5ea6\u3002",
        L"\u52a0\u901f\u623f\u95f4\u52a0\u8f7d\uff0c\u540c\u65f6\u4fdd\u7559\u5e73\u6ed1\u9ed1\u5c4f\u8fc7\u6e21\u3002",
        L"\u6781\u9650\u901f\u5ea6\uff1a\u65e0\u6de1\u5165\u6de1\u51fa\u5ef6\u8fdf\uff0c\u77ac\u95f4\u5207\u6362\u623f\u95f4\u3002"
    },
    /* ko */ {
        L"\uc6d0\ubcf8 \ubb38 \uc5f4\ub9bc \uc560\ub2c8\uba54\uc774\uc158\uc774 \uc815\uc0c1 \uc7ac\uc0dd\ub429\ub2c8\ub2e4.",
        L"\ubb38 \uc560\ub2c8\uba54\uc774\uc158\uc744 \uac74\ub108\ub6f0\uace0 \uae30\ubcf8 \uc18d\ub3c4\ub85c \ubc29\uc744 \ubd88\ub7ec\uc635\ub2c8\ub2e4.",
        L"\ubd80\ub4dc\ub7ec\uc6b4 \ud398\uc774\ub4dc\ub97c \uc720\uc9c0\ud558\uba74\uc11c \ubc29 \ub85c\ub529\uc744 \uac00\uc18d\ud569\ub2c8\ub2e4.",
        L"\ucd5c\uace0 \uc18d\ub3c4: \ud398\uc774\ub4dc \uc9c0\uc5f0 \uc5c6\uc774 \uc989\uc2dc \ubc29\uc744 \uc804\ud658\ud569\ub2c8\ub2e4."
    },
    /* ru */ {
        L"\u041e\u0440\u0438\u0433\u0438\u043d\u0430\u043b\u044c\u043d\u0430\u044f \u0430\u043d\u0438\u043c\u0430\u0446\u0438\u044f \u043e\u0442\u043a\u0440\u044b\u0442\u0438\u044f \u0434\u0432\u0435\u0440\u0435\u0439 \u0432\u043e\u0441\u043f\u0440\u043e\u0438\u0437\u0432\u043e\u0434\u0438\u0442\u0441\u044f \u043a\u0430\u043a \u043e\u0431\u044b\u0447\u043d\u043e.",
        L"\u041f\u0440\u043e\u043f\u0443\u0441\u043a\u0430\u0435\u0442 \u0430\u043d\u0438\u043c\u0430\u0446\u0438\u044e \u0434\u0432\u0435\u0440\u0435\u0439 \u0441\u043e \u0441\u0442\u0430\u043d\u0434\u0430\u0440\u0442\u043d\u043e\u0439 \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u043e\u0439 \u043a\u043e\u043c\u043d\u0430\u0442\u044b.",
        L"\u0423\u0441\u043a\u043e\u0440\u0435\u043d\u043d\u0430\u044f \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0430 \u043a\u043e\u043c\u043d\u0430\u0442 \u0441 \u0441\u043e\u0445\u0440\u0430\u043d\u0435\u043d\u0438\u0435\u043c \u043f\u043b\u0430\u0432\u043d\u043e\u0433\u043e \u0437\u0430\u0442\u0443\u0445\u0430\u043d\u0438\u044f.",
        L"\u041c\u0430\u043a\u0441\u0438\u043c\u0430\u043b\u044c\u043d\u0430\u044f \u0441\u043a\u043e\u0440\u043e\u0441\u0442\u044c: \u043c\u0433\u043d\u043e\u0432\u0435\u043d\u043d\u044b\u0439 \u043f\u0435\u0440\u0435\u0445\u043e\u0434 \u0431\u0435\u0437 \u0437\u0430\u0434\u0435\u0440\u0436\u043a\u0438 \u0437\u0430\u0442\u0443\u0445\u0430\u043d\u0438\u044f."
    },
};

inline const wchar_t* label(int lang, int mode)
{
    if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
    if (mode < 0 || mode > 3) mode = 0;
    return k_labels[lang][mode];
}

inline const wchar_t* desc(int lang, int mode)
{
    if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
    if (mode < 0 || mode > 3) mode = 0;
    return k_descs[lang][mode];
}

} // namespace cv_door_skip_text
