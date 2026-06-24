#pragma once

namespace Cheats
{
    struct CharacterEntry
    {
        int         id;           // Real ECharacterId value
        const char* name;
        int         maxVariation;
        int         chNum;        // Used only for emote base calculation
        const char* types[3];
    };

    static const CharacterEntry Characters[] =
    {
        {   1, "Izuku Midoriya",     1,   1,  { "Assault", "Strike",  nullptr } },
        {   2, "Katsuki Bakugo",     2,   3,  { "Strike",  "Rapid",   "Tech" } },
        {   3, "Ochaco Uraraka",     1,   3,  { "Rapid",   "Assault", nullptr } },
        {   4, "Shoto Todoroki",     1,   4,  { "Strike",  "Tech",    nullptr } },
        {   5, "Tenya Ida",          0,   5,  { "Rapid",   nullptr,   nullptr } },
        {   6, "Tsuyu Asui",         0,   6,  { "Rapid",   nullptr,   nullptr } },
        {   7, "Denki Kaminari",     1,   7,  { "Strike",  "Tech",    nullptr } },
        {   8, "Eijiro Kirishima",   1,   8,  { "Assault", "Strike",  nullptr } },
        {  10, "Momo Yaoyorozu",     0,  10,  { "Support", nullptr,   nullptr } },
        {  11, "Fumikage Tokoyami",  0,  11,  { "Assault", nullptr,   nullptr } },
        {  12, "All Might",          1,  12,  { "Assault", "Rapid",   nullptr } },
        {  13, "Shota Aizawa",       0,  13,  { "Tech",    nullptr,   nullptr } },
        {  15, "Tomura Shigaraki",   2,  15,  { "Strike",  "Assault", "Tech"  } },
        {  16, "All For One",        1,  16,  { "Tech",    "Strike",   nullptr } },
        {  17, "Dabi",               1,  17,  { "Tech",    "Strike",  nullptr } },
        {  18, "Himiko Toga",        1,  18,  { "Tech",    "Rapid",   nullptr } },
        {  23, "Endeavor",           1,  23,  { "Strike",  "Assault", nullptr } },
        {  24, "Mirio Togata",       1,  24,  { "Rapid",   "Tech",    nullptr } },
        {  25, "Nejire Hado",        1,  25,  { "Tech",    "Support", nullptr } },
        {  26, "Tamaki Amajiki",     0,  26,  { "Strike",  nullptr,   nullptr } },
        {  34, "Overhaul",           1,  34,  { "Support", "Assault", nullptr } },
        {  37, "Twice",              0,  37,  { "Rapid",   nullptr,   nullptr } },
        {  38, "Mr. Compress",       0,  38,  { "Support", nullptr,   nullptr } },
        {  43, "Hawks",              1,  43,  { "Rapid",   "Strike",  nullptr } },
        {  46, "Itsuka Kendo",       1,  46,  { "Assault", "Strike",  nullptr } },
        { 100, "Mt. Lady",           0, 100,  { "Assault", nullptr,   nullptr } },
        { 101, "Cementoss",          0, 101,  { "Support", nullptr,   nullptr } },
        { 102, "Ibara Shiozaki",     0, 102,  { "Support", nullptr,   nullptr } },
        { 103, "Kurogiri",           0, 103,  { "Support", nullptr,   nullptr } },
        { 104, "Neito Monoma",       0, 104,  { "Tech",    nullptr,   nullptr } },
        { 105, "Hitoshi Shinso",     0, 105,  { "Strike",  nullptr,   nullptr } },
        { 109, "Present Mic",        1, 109,  { "Strike",  "Tech",   nullptr}},
        { 111, "Mirko",              0, 111,  { "Rapic",  "strike",   nullptr } },
        { 114, "Star & Stripe",      0, 114,  { "Strike",  nullptr,   nullptr } },
        { 115, "Lady Nagant",        0, 115,  { "Strike",  nullptr,   nullptr } },
        { 200, "Armored All Might",  0, 200,  { "Tech",    nullptr,   nullptr } },
        { 201, "Prime All For One",  0, 201,  { "Assault", nullptr,   nullptr } },
        { 202, "Deku (Final)",       0, 202,  { "Rapid",   nullptr,   nullptr } },
        { 502, "Kota",               0, 502,  { "Strike",   nullptr,   nullptr } },
    };

    static const int CharacterCount = sizeof(Characters) / sizeof(Characters[0]);
}