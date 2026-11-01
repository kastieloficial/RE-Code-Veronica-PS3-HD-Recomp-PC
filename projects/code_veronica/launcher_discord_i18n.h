#pragma once
/* The Avancado option for the Discord presence (cv_discord.cpp), in the
 * launcher's languages (Lang order: pt en es ja fr de it pl tr ar zh ko ru). */
namespace cv_discord_text {
enum Id { OPTION, OPTION_DESC, COUNT };
inline constexpr const wchar_t* texts[13][COUNT] = {
{L"Mostrar no Discord", L"Enquanto você joga, o seu perfil do Discord mostra \"Jogando Resident Evil Code: Veronica X\". Precisa do Discord aberto no PC."},
{L"Show on Discord", L"While you play, your Discord profile shows \"Playing Resident Evil Code: Veronica X\". Needs the Discord app open on this PC."},
{L"Mostrar en Discord", L"Mientras juegas, tu perfil de Discord muestra \"Jugando a Resident Evil Code: Veronica X\". Necesita la app de Discord abierta en el PC."},
{L"Discordに表示", L"プレイ中、Discordのプロフィールに「Resident Evil Code: Veronica X をプレイ中」と表示されます。PCでDiscordアプリを起動しておく必要があります。"},
{L"Afficher sur Discord", L"Pendant que vous jouez, votre profil Discord affiche « Joue à Resident Evil Code: Veronica X ». L'application Discord doit être ouverte sur ce PC."},
{L"Auf Discord anzeigen", L"Während du spielst, zeigt dein Discord-Profil „Spielt Resident Evil Code: Veronica X“. Die Discord-App muss auf diesem PC geöffnet sein."},
{L"Mostra su Discord", L"Mentre giochi, il tuo profilo Discord mostra «Sta giocando a Resident Evil Code: Veronica X». Serve l'app Discord aperta su questo PC."},
{L"Pokaż na Discordzie", L"Podczas gry Twój profil na Discordzie pokazuje „Gra w Resident Evil Code: Veronica X”. Aplikacja Discord musi być otwarta na tym komputerze."},
{L"Discord'da göster", L"Oynarken Discord profilinde \"Resident Evil Code: Veronica X oynuyor\" görünür. Bu bilgisayarda Discord uygulaması açık olmalı."},
{L"إظهار على Discord", L"أثناء اللعب، يعرض ملفك على Discord عبارة \"يلعب Resident Evil Code: Veronica X\". يجب أن يكون تطبيق Discord مفتوحًا على هذا الحاسوب."},
{L"在 Discord 上显示", L"游戏时，你的 Discord 个人资料会显示“正在玩 Resident Evil Code: Veronica X”。需要在这台电脑上打开 Discord 应用。"},
{L"Discord에 표시", L"플레이하는 동안 Discord 프로필에 \"Resident Evil Code: Veronica X 플레이 중\"이 표시됩니다. 이 PC에서 Discord 앱이 실행 중이어야 합니다."},
{L"Показывать в Discord", L"Во время игры в вашем профиле Discord видно «Играет в Resident Evil Code: Veronica X». Нужно открытое приложение Discord на этом ПК."},
};
}
