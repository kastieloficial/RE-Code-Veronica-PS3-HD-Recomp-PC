// Shared labels of the port's menus (footers, header), in the launcher's 13 languages (same order as launcher_i18n.h).
// No Portuguese fallback: complete() below fails the build when a cell is empty.
#pragma once
namespace cv_port_ui_i18n {
enum Id {NAVIGATE,CATEGORY,PUT,REMOVE,BACK,CLOSE,CHOOSE,CONFIRM,TABS,SCROLL_LIST,HOLD_FAST,GAME_PAUSED,PAUSED_CAPS,COUNT};
inline constexpr const wchar_t* strings[13][COUNT]={
{L"Navegar",L"Categoria",L"Colocar",L"Remover",L"Voltar",L"Fechar",L"Escolher",L"Confirmar",L"Abas",L"Rolar lista",L"Segure ↑ ou ↓ para percorrer mais rápido",L"Jogo pausado",L"PAUSADO"},
{L"Navigate",L"Category",L"Put",L"Remove",L"Back",L"Close",L"Choose",L"Confirm",L"Tabs",L"Scroll list",L"Hold ↑ or ↓ to scroll faster",L"Game paused",L"PAUSED"},
{L"Navegar",L"Categoría",L"Colocar",L"Quitar",L"Volver",L"Cerrar",L"Elegir",L"Confirmar",L"Pestañas",L"Recorrer lista",L"Mantén ↑ o ↓ para recorrer más rápido",L"Juego en pausa",L"EN PAUSA"},
{L"選択",L"カテゴリー",L"入れる",L"外す",L"戻る",L"閉じる",L"選ぶ",L"決定",L"タブ",L"リスト移動",L"↑／↓を長押しすると速く移動します",L"ゲーム一時停止中",L"一時停止"},
{L"Naviguer",L"Catégorie",L"Placer",L"Retirer",L"Retour",L"Fermer",L"Choisir",L"Confirmer",L"Onglets",L"Parcourir la liste",L"Maintenez ↑ ou ↓ pour parcourir plus vite",L"Jeu en pause",L"EN PAUSE"},
{L"Navigieren",L"Kategorie",L"Ablegen",L"Entfernen",L"Zurück",L"Schließen",L"Wählen",L"Bestätigen",L"Reiter",L"Liste blättern",L"↑ oder ↓ halten, um schneller zu blättern",L"Spiel pausiert",L"PAUSIERT"},
{L"Naviga",L"Categoria",L"Metti",L"Rimuovi",L"Indietro",L"Chiudi",L"Scegli",L"Conferma",L"Schede",L"Scorri elenco",L"Tieni premuto ↑ o ↓ per scorrere più velocemente",L"Gioco in pausa",L"IN PAUSA"},
{L"Nawigacja",L"Kategoria",L"Połóż",L"Usuń",L"Wróć",L"Zamknij",L"Wybierz",L"Potwierdź",L"Karty",L"Przewijaj listę",L"Przytrzymaj ↑ lub ↓, aby przewijać szybciej",L"Gra wstrzymana",L"PAUZA"},
{L"Gezin",L"Kategori",L"Yerleştir",L"Kaldır",L"Geri",L"Kapat",L"Seç",L"Onayla",L"Sekmeler",L"Listeyi kaydır",L"Daha hızlı kaydırmak için ↑ veya ↓ basılı tutun",L"Oyun duraklatıldı",L"DURAKLATILDI"},
{L"تنقل",L"الفئة",L"وضع",L"إزالة",L"رجوع",L"إغلاق",L"اختيار",L"تأكيد",L"علامات التبويب",L"تمرير القائمة",L"اضغط مطولاً على ↑ أو ↓ للتمرير أسرع",L"اللعبة متوقفة مؤقتاً",L"متوقف مؤقتاً"},
{L"选择",L"分类",L"放入",L"移除",L"返回",L"关闭",L"选择",L"确认",L"选项卡",L"滚动列表",L"按住 ↑ 或 ↓ 可加快滚动",L"游戏已暂停",L"已暂停"},
{L"이동",L"분류",L"넣기",L"제거",L"뒤로",L"닫기",L"선택",L"확인",L"탭",L"목록 스크롤",L"↑ 또는 ↓를 길게 누르면 빠르게 이동합니다",L"게임 일시 정지",L"일시 정지"},
{L"Навигация",L"Категория",L"Положить",L"Убрать",L"Назад",L"Закрыть",L"Выбрать",L"Подтвердить",L"Вкладки",L"Прокрутка списка",L"Удерживайте ↑ или ↓ для быстрой прокрутки",L"Игра на паузе",L"ПАУЗА"},
};
static_assert(sizeof(strings)/sizeof(strings[0])==13);
constexpr bool complete(){for(const auto& row:strings)for(const auto* value:row)if(!value||!*value)return false;return true;}
static_assert(complete(),"Every port UI label must be translated in all launcher languages");
inline const wchar_t* tr(int lang,Id id){return strings[lang<0?0:lang>12?12:lang][id];}
}
