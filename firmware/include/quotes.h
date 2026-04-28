#pragma once

// Offline fallback quotes — used when WiFi is unavailable or the fetch fails.
// When online, fresh quotes come from zenquotes.io every 30 minutes.

static const char* const QUOTES[] = {
    "The only way to do great work is to love what you do.",
    "In the middle of every difficulty lies opportunity.",
    "It does not matter how slowly you go as long as you do not stop.",
    "Life is what happens when you're busy making other plans.",
    "The future belongs to those who believe in the beauty of their dreams.",
    "Not all those who wander are lost.",
    "You miss 100% of the shots you don't take.",
    "Whether you think you can or you think you can't, you're right.",
    "The only impossible journey is the one you never begin.",
    "In the end, it's not the years in your life that count. It's the life in your years.",
    "Be yourself; everyone else is already taken.",
    "Two roads diverged in a wood, and I took the one less traveled by.",
    "To be yourself in a world that is constantly trying to make you something else is the greatest accomplishment.",
    "In three words I can sum up everything I've learned about life: it goes on.",
    "To live is the rarest thing in the world. Most people just exist.",
    "Success is not final, failure is not fatal: it is the courage to continue that counts.",
    "The way to get started is to quit talking and begin doing.",
    "If life were predictable it would cease to be life, and be without flavor.",
    "Spread love everywhere you go. Let no one ever come to you without leaving happier.",
    "When you reach the end of your rope, tie a knot in it and hang on.",
    "Always remember that you are absolutely unique. Just like everyone else.",
    "Do not go where the path may lead, go instead where there is no path and leave a trail.",
    "You will face many defeats in life, but never let yourself be defeated.",
    "The greatest glory in living lies not in never falling, but in rising every time we fall.",
    "In the end, it's not the years in your life that count. It's the life in your years.",
    "Never let the fear of striking out keep you from playing the game.",
    "Life is either a daring adventure or nothing at all.",
    "Many of life's failures are people who did not realize how close they were to success when they gave up.",
    "You have brains in your head. You have feet in your shoes. You can steer yourself any direction you choose.",
    "If you look at what you have in life, you'll always have more.",
};

static const uint8_t QUOTE_COUNT = sizeof(QUOTES) / sizeof(QUOTES[0]);
