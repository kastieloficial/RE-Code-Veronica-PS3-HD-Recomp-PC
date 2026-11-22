// Texts of the return-to-title box (cv_title_return_ui.cpp) and its notices, in the launcher's 13 languages (same order as launcher_i18n.h).
// The box's footer labels (Choose / Confirm / Back) come from cv_port_ui_i18n.h. No fallback language: complete() fails the build on an empty cell.
#pragma once
namespace cv_title_return_i18n {
enum Id {HEADER,QUESTION,WARNING,YES,NO,BLOCKED,RETURNING,COUNT};
inline constexpr const wchar_t* strings[13][COUNT]={
{L"CODE: VERONICA · JOGO PAUSADO",L"Voltar para a tela de título?",L"O progresso não salvo será perdido.",L"Sim",L"Não",L"Voltar ao título não está disponível ao salvar, carregar ou em cenas.",L"Voltando para a tela de título…"},
{L"CODE: VERONICA · GAME PAUSED",L"Return to the title screen?",L"Unsaved progress will be lost.",L"Yes",L"No",L"Returning to the title is not available while saving, loading or during cutscenes.",L"Returning to the title screen…"},
{L"CODE: VERONICA · JUEGO EN PAUSA",L"¿Volver a la pantalla de título?",L"Se perderá el progreso no guardado.",L"Sí",L"No",L"Volver al título no está disponible al guardar, cargar ni durante las escenas.",L"Volviendo a la pantalla de título…"},
{L"CODE: VERONICA · 一時停止中",L"タイトル画面に戻りますか？",L"セーブしていない進行状況は失われます。",L"はい",L"いいえ",L"セーブ中、ロード中、およびムービー中はタイトルに戻れません。",L"タイトル画面に戻っています…"},
{L"CODE: VERONICA · JEU EN PAUSE",L"Retourner à l'écran titre ?",L"La progression non sauvegardée sera perdue.",L"Oui",L"Non",L"Le retour à l'écran titre n'est pas disponible pendant une sauvegarde, un chargement ou une cinématique.",L"Retour à l'écran titre…"},
{L"CODE: VERONICA · SPIEL PAUSIERT",L"Zum Titelbildschirm zurückkehren?",L"Nicht gespeicherter Fortschritt geht verloren.",L"Ja",L"Nein",L"Die Rückkehr zum Titelbildschirm ist beim Speichern, Laden oder in Zwischensequenzen nicht möglich.",L"Rückkehr zum Titelbildschirm …"},
{L"CODE: VERONICA · GIOCO IN PAUSA",L"Tornare alla schermata del titolo?",L"I progressi non salvati andranno persi.",L"Sì",L"No",L"Il ritorno al titolo non è disponibile durante il salvataggio, il caricamento o i filmati.",L"Ritorno alla schermata del titolo…"},
{L"CODE: VERONICA · GRA WSTRZYMANA",L"Wrócić do ekranu tytułowego?",L"Niezapisany postęp zostanie utracony.",L"Tak",L"Nie",L"Powrót do ekranu tytułowego nie jest dostępny podczas zapisu, wczytywania ani w scenach filmowych.",L"Powrót do ekranu tytułowego…"},
{L"CODE: VERONICA · OYUN DURAKLATILDI",L"Başlık ekranına dönülsün mü?",L"Kaydedilmemiş ilerleme kaybolacak.",L"Evet",L"Hayır",L"Kaydetme, yükleme ve ara sahneler sırasında başlık ekranına dönülemez.",L"Başlık ekranına dönülüyor…"},
{L"CODE: VERONICA · اللعبة متوقفة مؤقتاً",L"العودة إلى شاشة العنوان؟",L"سيتم فقدان التقدم غير المحفوظ.",L"نعم",L"لا",L"العودة إلى شاشة العنوان غير متاحة أثناء الحفظ أو التحميل أو المشاهد السينمائية.",L"جارٍ العودة إلى شاشة العنوان…"},
{L"CODE: VERONICA · 游戏已暂停",L"返回标题画面？",L"未保存的进度将会丢失。",L"是",L"否",L"存档、读档或过场动画期间无法返回标题画面。",L"正在返回标题画面…"},
{L"CODE: VERONICA · 게임 일시 정지",L"타이틀 화면으로 돌아가시겠습니까?",L"저장하지 않은 진행 상황은 사라집니다.",L"예",L"아니요",L"저장, 불러오기 또는 컷신 중에는 타이틀 화면으로 돌아갈 수 없습니다.",L"타이틀 화면으로 돌아가는 중…"},
{L"CODE: VERONICA · ИГРА НА ПАУЗЕ",L"Вернуться на титульный экран?",L"Несохранённый прогресс будет потерян.",L"Да",L"Нет",L"Возврат на титульный экран недоступен во время сохранения, загрузки и в роликах.",L"Возврат на титульный экран…"},
};
static_assert(sizeof(strings)/sizeof(strings[0])==13);
constexpr bool complete(){for(const auto& row:strings)for(const auto* value:row)if(!value||!*value)return false;return true;}
static_assert(complete(),"Every return-to-title text must be translated in all launcher languages");
inline const wchar_t* tr(int lang,Id id){return strings[lang<0?0:lang>12?12:lang][id];}
}
