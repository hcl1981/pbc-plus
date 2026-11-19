/* story.c -- the story: twenty missions, told by four people.
 *
 * Not a file of its own for the compiler: game.c includes it (when RT_STORY
 * is on), because a mission is made of the same things as the jobs at the
 * phones -- targets, a clock, a car to fetch, the police. Take RT_STORY
 * away and the free game is all there is; nothing else changes.
 *
 * A mission is a list of steps. Each step waits for something (be there,
 * stop there, get into that car, lose the police ...) or does something at
 * once (set the clock, raise the heat, open the ferry) and falls through to
 * the next.
 */

enum {
    PL_SQUARE, PL_HARBOUR, PL_TOWER, PL_HOSPITAL, PL_POLICE, PL_PAINT, PL_GARAGE, PL_GUNS,
    PL_PIER, PL_PIER_PALM, PL_WORKS, PL_BAR, PL_BRIDGE,
    PL_NEAR, PL_MID, PL_FAR,            /* some road that far from where the player is */
};
enum {
    OP_END,
    OP_GO,      /* a: place. Be there */
    OP_STOP,    /* a: place. Stop there with a car */
    OP_FARE,    /* a: place. Stop there with a car; somebody gets in */
    OP_CAR,     /* a: model, b: colour, n: place. Find that car and get in */
    OP_BRING,   /* a: place. Stop there in that car; it is handed over */
    OP_LOSE,    /* until no star is left */
    OP_KILL,    /* a: model. That car drives about town: wreck it */
    OP_TAIL,    /* n: seconds. Find the black sedan and stay behind it unseen */
    OP_DROPS,   /* a: place, n: how many (up to 4). Visit them all */
    OP_HOLD,    /* a: place, n: seconds. Stay close to it that long */
    /* done at once: */
    OP_HEAT,    /* n: so much heat */
    OP_TIME,    /* n: seconds for what follows, 0: no clock */
    OP_OPEN,    /* a: 0 the ferry, 1 the long bridge */
    OP_GUN,     /* n: rounds */
};
typedef struct { uint8_t op, a, b; int16_t n; const char *text[2]; } step_t;    /* text: english, deutsch */
typedef struct {
    uint8_t who;
    const char *title[2], *brief[2][4], *done[2];   /* each english, deutsch */
    int16_t reward;
    const step_t *steps;
} mission_t;

enum { ROSA, DAVE, LOLA, GRAY };
static const struct { const char *name; uint8_t place, look; } people[] = {
    [ROSA] = { "ROSA MARCHETTI", PL_SQUARE, 1 },
    [DAVE] = { "DAVE HOLLIS", PL_HARBOUR, 30 },
    [LOLA] = { "LOLA VANCE", PL_BAR, 20 },
    [GRAY] = { "MR. GRAY", PL_WORKS, 31 },
};

#define STEPS(name, ...) static const step_t name[] = { __VA_ARGS__, { OP_END, 0, 0, 0, { 0, 0 } } }

STEPS(m01,
      { OP_TIME, 0, 0, 45, { 0, 0 } },
      { OP_GO, PL_MID, 0, 0, { "DELIVER THE ORDER", "BESTELLUNG LIEFERN" } });
STEPS(m02,
      { OP_FARE, PL_HARBOUR, 0, 0, { "FETCH TINO BY CAR", "TINO MIT DEM AUTO HOLEN" } },
      { OP_STOP, PL_SQUARE, 0, 0, { "BRING HIM TO ROSA", "BRING IHN ZU ROSA" } });
STEPS(m03,
      { OP_KILL, CAR_VAN, 0, 0, { "WRECK THE PIZZA VAN", "PIZZAWAGEN ZERLEGEN" } });
STEPS(m04,
      { OP_TIME, 0, 0, 90, { 0, 0 } },
      { OP_DROPS, PL_SQUARE, 0, 4, { "COLLECT AT 4 PLACES", "AN 4 ORTEN KASSIEREN" } });
STEPS(m05,
      { OP_TAIL, 0, 0, 30, { "FIND THE BLACK SEDAN", "SCHWARZE LIMOUSINE FINDEN" } });
STEPS(m06,
      { OP_GO, PL_HARBOUR, 0, 0, { "MEET DAVE AT THE HARBOUR", "TRIFF DAVE AM HAFEN" } });
STEPS(m07,
      { OP_DROPS, PL_HARBOUR, 0, 3, { "FIND 3 FERRY PARTS", "3 FÄHRENTEILE FINDEN" } });
STEPS(m08,
      { OP_CAR, CAR_TRUCK, 2, PL_FAR, { "FIND DAVE'S LORRY", "DAVES LASTER FINDEN" } },
      { OP_HEAT, 0, 0, 110, { 0, 0 } },
      { OP_BRING, PL_HARBOUR, 0, 0, { "LORRY TO THE HARBOUR", "LASTER ZUM HAFEN" } });
STEPS(m09,
      { OP_HEAT, 0, 0, 230, { 0, 0 } },
      { OP_LOSE, 0, 0, 0, { "LOSE THE POLICE", "POLIZEI ABHÄNGEN" } });
STEPS(m10,
      { OP_OPEN, 0, 0, 0, { 0, 0 } },
      { OP_GO, PL_BAR, 0, 0, { "FERRY TO LOLA'S BAR", "MIT DER FÄHRE ZU LOLAS BAR" } });
STEPS(m11,
      { OP_FARE, PL_NEAR, 0, 0, { "FETCH THE GUESTS BY CAR", "GÄSTE MIT AUTO HOLEN" } },
      { OP_TIME, 0, 0, 50, { 0, 0 } },
      { OP_STOP, PL_PIER_PALM, 0, 0, { "TO THE PIER, QUICK", "ZUM ANLEGER, SCHNELL" } });
STEPS(m12,
      { OP_CAR, CAR_SPORT, CAR_COLOURS - 1, PL_NEAR, { "FIND THE PINK RACER", "ROSA RENNER FINDEN" } },
      { OP_HEAT, 0, 0, 110, { 0, 0 } },
      { OP_BRING, PL_BAR, 0, 0, { "PARK IT AT LOLA'S", "BEI LOLA ABSTELLEN" } });
STEPS(m13,
      { OP_KILL, CAR_BIKE, 0, 0, { "STOP THE COURIER", "KURIER STOPPEN" } });
STEPS(m14,
      { OP_GO, PL_SQUARE, 0, 0, { "INVOICE TO THE SQUARE", "RECHNUNG ZUM PLATZ" } },
      { OP_OPEN, 1, 0, 0, { 0, 0 } });
STEPS(m15,
      { OP_GO, PL_WORKS, 0, 0, { "CROSS TO THE WORKS", "ZUM WERK HINÜBER" } });
STEPS(m16,
      { OP_CAR, CAR_VAN, 3, PL_MID, { "FIND THE PAYROLL VAN", "LOHNWAGEN FINDEN" } },
      { OP_HEAT, 0, 0, 110, { 0, 0 } },
      { OP_BRING, PL_WORKS, 0, 0, { "VAN TO THE WORKS", "WAGEN ZUM WERK" } });
STEPS(m17,
      { OP_KILL, CAR_TRUCK, 0, 0, { "STOP THE FIRST LORRY", "ERSTEN LASTER STOPPEN" } },
      { OP_KILL, CAR_TRUCK, 0, 0, { "AND THE SECOND", "UND DEN ZWEITEN" } });
STEPS(m18,
      { OP_TAIL, 0, 0, 35, { "FIND THE BLACK SEDAN", "SCHWARZE LIMOUSINE FINDEN" } });
STEPS(m19,
      { OP_FARE, PL_SQUARE, 0, 0, { "FETCH THE CREW BY CAR", "CREW MIT AUTO ABHOLEN" } },
      { OP_STOP, PL_TOWER, 0, 0, { "DRIVE THEM TO THE TOWER", "ZUM TOWER FAHREN" } });
STEPS(m20,
      { OP_GUN, 0, 0, 24, { 0, 0 } },
      { OP_HEAT, 0, 0, 230, { 0, 0 } },
      { OP_HOLD, PL_TOWER, 0, 30, { "HOLD OUT AT THE TOWER", "AM TOWER AUSHARREN" } },
      { OP_STOP, PL_HARBOUR, 0, 0, { "NOW THE HARBOUR. GO", "JETZT ZUM HAFEN. LOS" } });

static const mission_t missions[] = {
    { ROSA, { "A SLOW NIGHT", "EINE RUHIGE NACHT" },
      { { "You drive? Good. My last", "driver found religion.", "Take this to a customer.", "Hot food, cold cash." },
        { "Du fährst? Gut. Mein", "letzter Fahrer wurde", "fromm. Bring das einem", "Kunden. Heiß, gegen bar." } },
      { "Rosa: You came back. Rare.", "Rosa: Du kamst zurück. Rar." }, 80, m01 },
    { ROSA, { "FAMILY", "FAMILIE" },
      { { "My nephew Tino is at the", "harbour. He thinks he is", "a sailor. Bring him home", "before he learns knots." },
        { "Mein Neffe Tino ist am", "Hafen. Er hält sich für", "einen Seemann. Hol ihn,", "bevor er Knoten lernt." } },
      { "Tino is grounded. Ashore.", "Tino hat Hausarrest. An Land." }, 120, m02 },
    { ROSA, { "THE COMPETITION", "DIE KONKURRENZ" },
      { { "A van sells pizza at my", "door. Hale's cousin owns", "it. Make it stop. The van,", "I mean. Only the van." },
        { "Vor meiner Tür verkauft", "ein Wagen Pizza. Er gehört", "Hales Cousin. Sorg dafür,", "dass er aufhört: der Wagen" } },
      { "Rosa: A free market. Ha.", "Rosa: Freier Markt. Ha." }, 180, m03 },
    { ROSA, { "COLLECTION DAY", "ZAHLTAG" },
      { { "Four friends owe me for", "catering. They forget.", "Remind them. Be polite,", "and be quick." },
        { "Vier Freunde schulden mir", "Geld fürs Catering. Sie", "vergessen es. Erinnere", "sie. Höflich und zügig." } },
      { "Rosa: Polite works. Mostly.", "Rosa: Höflich wirkt. Meist." }, 200, m04 },
    { ROSA, { "WHO IS HALE", "WER IST HALE" },
      { { "Commissioner Hale takes a", "cut of every plate I sell.", "His bagman drives a black", "sedan. See where he goes." },
        { "Kommissar Hale kassiert", "an jedem Teller mit. Sein", "Geldbote fährt schwarze", "Limousine. Sieh, wohin." } },
      { "The Tower. Of course.", "Zum Tower. Natürlich." }, 220, m05 },
    { ROSA, { "A FRIEND AT THE DOCKS", "EIN FREUND AM HAFEN" },
      { { "Dave Hollis runs the", "harbour. Hale shut his", "ferry 'for repairs'. Go.", "Say Rosa sent you." },
        { "Dave Hollis führt den", "Hafen. Hale ließ seine", "Fähre 'reparieren'. Geh", "hin. Rosa schickt dich." } },
      { "Dave: Rosa sent you? Good.", "Dave: Rosa schickt dich? Gut." }, 60, m06 },
    { DAVE, { "SPARE PARTS", "ERSATZTEILE" },
      { { "The ferry needs three", "parts. Hale's men hid", "them around town. I know", "where. You have a car." },
        { "Der Fähre fehlen drei", "Teile. Hales Leute haben", "sie versteckt. Ich weiß,", "wo. Du hast ein Auto." } },
      { "Dave: Two fit. One will learn.", "Dave: Zwei passen. Eins lernt." }, 200, m07 },
    { DAVE, { "NIGHT FREIGHT", "NACHTFRACHT" },
      { { "A lorry of mine stands", "where it should not.", "Bring it here before the", "police count the boxes." },
        { "Ein Laster von mir steht,", "wo er nicht stehen soll.", "Bring ihn her, bevor die", "Polizei Kisten zählt." } },
      { "Dave: Nobody counted. Fine.", "Dave: Keiner hat gezählt." }, 280, m08 },
    { DAVE, { "CUSTOMS", "ZOLL" },
      { { "Customs took an interest", "in me. Take their", "interest for a long drive", "and come back without it." },
        { "Der Zoll interessiert", "sich für mich. Nimm sein", "Interesse mit auf eine", "lange Fahrt. Komm ohne." } },
      { "Dave: They found nothing.", "Dave: Nichts gefunden." }, 300, m09 },
    { DAVE, { "FIRST SAILING", "ERSTE FAHRT" },
      { { "She floats. Mostly. The", "ferry runs again, to Palm", "Key. Lola Vance has a bar", "there. You can trust her." },
        { "Sie schwimmt. Meistens.", "Die Fähre fährt wieder,", "nach Palm Key. Lola Vance", "hat da eine Bar. Trau ihr." } },
      { "Lola: Dave's driver? Sit.", "Lola: Daves Fahrer? Setz dich." }, 150, m10 },
    { LOLA, { "SUNBURN", "SONNENBRAND" },
      { { "Two guests of mine missed", "the ferry and lost their", "dignity. Fetch them, then", "the pier. The boat is due." },
        { "Zwei Gäste haben Fähre", "und Würde verpasst. Hol", "sie, dann zum Anleger.", "Das Boot wartet nicht." } },
      { "Lola: They tipped. In sand.", "Lola: Trinkgeld. In Sand." }, 200, m11 },
    { LOLA, { "THE COLLECTOR", "DER SAMMLER" },
      { { "A man here owes me a car.", "He disagrees. It is pink,", "one cannot miss it. Park", "it behind my bar." },
        { "Ein Mann hier schuldet", "mir ein Auto. Er sieht das", "anders. Es ist rosa. Stell", "es hinter meine Bar." } },
      { "Lola: Pink suits the bar.", "Lola: Rosa passt zur Bar." }, 300, m12 },
    { LOLA, { "PAPER TRAIL", "PAPIERSPUR" },
      { { "Hale built the bridge to", "Iron Isle. Twice, on", "paper. A courier has the", "second bill. Stop him." },
        { "Hale baute die Brücke zur", "Iron Isle. Zweimal, auf", "Papier. Ein Kurier hat die", "zweite Rechnung. Halt ihn." } },
      { "Two bridges, one strait. Art.", "Zwei Brücken, ein Sund. Kunst." }, 280, m13 },
    { LOLA, { "HEADLINES", "SCHLAGZEILEN" },
      { { "Take the bill to the news", "stand at the Square. By", "morning the bridge opens.", "Shame is a fine engineer." },
        { "Bring die Rechnung zum", "Kiosk am Platz. Morgen", "ist die Brücke offen.", "Scham baut schneller." } },
      { "The long bridge is open.", "Die lange Brücke ist offen." }, 250, m14 },
    { LOLA, { "THE WORKS", "DAS WERK" },
      { { "Mr. Gray runs the Works", "on Iron Isle. He prints", "what Hale spends. He may", "be tired of it. Ask him." },
        { "Mr. Gray führt das Werk", "auf Iron Isle. Er druckt,", "was Hale ausgibt. Er hat", "es vielleicht satt. Frag." } },
      { "Gray: I expected taller.", "Gray: Ich erwartete Größeres." }, 100, m15 },
    { GRAY, { "OVERTIME", "ÜBERSTUNDEN" },
      { { "I am an honest forger.", "Hale pays in promises.", "His payroll van is out", "there. Bring it. Gently." },
        { "Ich bin ein ehrlicher", "Fälscher. Hale zahlt mit", "Versprechen. Sein Lohn-", "wagen fährt herum. Hol ihn" } },
      { "Gray: Real money. How quaint.", "Gray: Echtes Geld. Wie putzig." }, 350, m16 },
    { GRAY, { "SCRAP VALUE", "SCHROTTWERT" },
      { { "Hale sent two lorries for", "my presses. I like my", "presses here. See that", "the lorries do not arrive." },
        { "Hale schickt zwei Laster", "für meine Pressen. Ich mag", "meine Pressen hier. Sorg,", "dass sie nie ankommen." } },
      { "Gray: Lost in transit. Pity.", "Gray: Unterwegs verloren. Pech" }, 400, m17 },
    { GRAY, { "THE LEDGER", "DAS BUCH" },
      { { "Every bribe is in a book", "in the Tower. The bagman", "carries the key. Follow", "him. Do not wave." },
        { "Jede Bestechung steht in", "einem Buch im Tower. Der", "Geldbote hat den Schlüssel", "Folg ihm. Winke nicht." } },
      { "The key hangs on his mirror.", "Der Schlüssel hängt am Spiegel" }, 350, m18 },
    { GRAY, { "HANDS", "HÄNDE" },
      { { "You need hands. Tino and", "a locksmith wait at the", "Square. Yes, that Tino.", "Rosa insists." },
        { "Du brauchst Hände. Tino", "und ein Schlosser warten", "am Platz. Ja, der Tino.", "Rosa besteht darauf." } },
      { "Tino brought sandwiches.", "Tino hat Brote mitgebracht." }, 200, m19 },
    { ROSA, { "THE TOWER", "DER TOWER" },
      { { "Keep the engine warm at", "the Tower while they work.", "Then the harbour. Dave", "has the boat. Do not stop." },
        { "Halt am Tower den Motor", "warm, solange sie drin", "sind. Dann zum Hafen. Dave", "hat das Boot. Halt nie an." } },
      { "Hale resigned. Tower for sale.", "Hale ist weg. Tower zu haben." }, 3000, m20 },
};
#define MISSIONS ((int)(sizeof missions / sizeof missions[0]))

int story_count(void) { return MISSIONS; }

/* The chapters, which are the story's levels: Rosa's missions, Dave's,
 * Lola's, and Gray's with the last one. A chapter comes with five lives. */
int story_chapter(int mission)
{
    static const uint8_t first[] = { 0, 6, 10, 15 };
    int c = 0;
    for (int i = 1; i < 4; i++)
        if (mission >= first[i])
            c = first[i];
    return c;
}

const char *story_chapter_name(int mission)
{
    return people[missions[story_chapter(mission < MISSIONS ? mission : MISSIONS - 1)].who].name;
}

/* what a mission pays: a quarter less for every arrest while on it */
static int story_pay(const mission_t *m)
{
    return m->reward * (4 - (G.fails > 3 ? 3 : G.fails)) / 4;
}

/* a scene before the briefing of a mission, where the story turns */
static int card_before(int mission)
{
    switch (mission) {
    case 0: return PIC_TRATTORIA;
    case 4: return PIC_TOWER;
    case 6: return PIC_HARBOUR;
    case 9: return PIC_FERRY;
    case 10: return PIC_BAR;
    case 13: return PIC_BRIDGE;
    case 15: return PIC_WORKS;
    case 19: return PIC_GETAWAY;
    default: return -1;
    }
}

const char *story_caption(int pic)
{
    switch (pic) {
    case PIC_ARRIVAL:   return L("NO MONEY. ONE TALENT.", "KEIN GELD. EIN TALENT.");
    case PIC_TRATTORIA: return L("ROSA'S, AFTER CLOSING", "BEI ROSA, NACH LADENSCHLUSS");
    case PIC_TOWER:     return L("THE TOWER. HALE'S.", "DER TOWER. HALES.");
    case PIC_HARBOUR:   return L("THE HARBOUR. NOTHING SAILS.", "DER HAFEN. NICHTS FÄHRT.");
    case PIC_FERRY:     return L("FIRST SAILING", "ERSTE FAHRT");
    case PIC_BAR:       return L("PALM KEY. LOLA'S.", "PALM KEY. BEI LOLA.");
    case PIC_BRIDGE:    return L("THE LONG BRIDGE OPENS", "DIE LANGE BRÜCKE ÖFFNET");
    case PIC_WORKS:     return L("IRON ISLE. THE WORKS.", "IRON ISLE. DAS WERK.");
    case PIC_GETAWAY:   return L("ONE LAST DRIVE", "EINE LETZTE FAHRT");
    default:            return "";
    }
}

int story_portrait(void)
{
    return PIC_ROSA + missions[G.talk >= 0 && G.talk < MISSIONS ? G.talk : 0].who;
}

/* a new story begins with a picture of the arrival */
static void story_opening(void)
{
    if (!RT_ART || G.mode != MODE_STORY || G.story || G.cash || G.jobs)
        return;
    G.card = PIC_ARRIVAL;
    G.talk = -1;
    G.state = ST_TALK;
}

static void place_xy(int id, float *x, float *y)
{
    static const spot_t *const fixed[] = {
        [PL_SQUARE] = &w_square, [PL_HARBOUR] = &w_harbour, [PL_TOWER] = &w_tower, [PL_HOSPITAL] = &w_hospital,
        [PL_POLICE] = &w_police, [PL_PAINT] = &w_paint[0], [PL_GARAGE] = &w_garage, [PL_GUNS] = &w_ammo,
        [PL_PIER] = &w_ferry[0], [PL_PIER_PALM] = &w_ferry[1], [PL_WORKS] = &w_works, [PL_BAR] = &w_bar,
        [PL_BRIDGE] = &w_bridge,
    };
    if (id >= PL_NEAR) {
        float lo = id == PL_NEAR ? 150 : id == PL_MID ? 300 : 480;
        road_point(px(), py(), lo, lo * 1.6f + 60, x, y);
        return;
    }
    *x = (float)fixed[id]->x;
    *y = (float)fixed[id]->y;
}

/* the road nearest to a place: for everything that is done by car */
static void road_near(float x, float y, float *rx, float *ry)
{
    float bd = 1e12f;
    *rx = x;
    *ry = y;
    for (int n = 0; n < nnodes; n++)
        for (int d = D_E; d <= D_S; d++) {
            if (nodes[n].e[d] == NO_NODE)
                continue;
            const node_t *a = &nodes[n], *b = &nodes[nodes[n].e[d]];
            for (int k = 1; k <= 3; k++) {
                float mx = (float)a->x + (float)(b->x - a->x) * (float)k * 0.25f;
                float my = (float)a->y + (float)(b->y - a->y) * (float)k * 0.25f;
                float dd = dist2(mx, my, x, y);
                if (dd < bd && t_road(tile_px(mx, my))) {
                    bd = dd;
                    *rx = mx;
                    *ry = my;
                }
            }
        }
}

bool story_giver(giver_t *g)
{
    if (G.mode != MODE_STORY || G.story >= MISSIONS)
        return false;
    const mission_t *m = &missions[G.story];
    g->name = people[m->who].name;
    g->look = people[m->who].look;
    place_xy(people[m->who].place, &g->x, &g->y);
    return true;
}

void story_brief(const char **name, const char **title, const char *const **lines, int *reward)
{
    const mission_t *m = &missions[G.talk < MISSIONS ? G.talk : 0];
    *name = people[m->who].name;
    *title = m->title[rt_lang];
    *lines = m->brief[rt_lang];
    *reward = story_pay(m);
}

static void story_clear(void)
{
    if (G.job_car >= 0) {
        cars[G.job_car].keep = 0;
        cars[G.job_car].tail = 0;
        G.job_car = -1;
    }
    for (int i = 0; i < PEDS; i++)
        if (peds[i].used && peds[i].state == P_WAIT)
            peds[i].state = P_WALK;
    G.job = J_NONE;
    targets_clear();
    G.timed = G.time_wins = false;
    G.job_text[0] = 0;
    G.phone_wait = 150;
    G.story_wait = 200;
}

static void story_win(void)
{
    const mission_t *m = &missions[G.story];
    char s[48];
    int reward = story_pay(m);
    story_clear();
    G.fails = 0;
    snprintf(s, sizeof s, L("MISSION DONE +$%d", "MISSION ERLEDIGT +$%d"), reward);
    message(s, RGB(120, 255, 120), 170);
    note(m->done[rt_lang]);
    G.note_t = 260;
    snd_sfx(SFX_DONE, 255);
    pay(reward);
    if (G.story + 1 < MISSIONS && story_chapter(G.story + 1) != story_chapter(G.story))
        G.lives = LIVES;                        /* a new chapter */
    if (++G.story == MISSIONS)
        message(L("THE END. SO FAR.", "ENDE. VORERST."), RGB(255, 220, 64), 400);
    save();
}

static void one_target(float x, float y)
{
    targets_clear();
    G.tgt[0].x = x;
    G.tgt[0].y = y;
    G.tgt[0].on = true;
}

/* a car that drives about for the mission: to be followed, or wrecked */
static bool story_car(int model, bool runs)
{
    int c = -1;
    for (int tries = 0; c < 0 && tries < 60; tries++)
        c = spawn_driving(model, 4, AI_TRAFFIC);
    if (c < 0)
        return false;
    cars[c].keep = 1;
    cars[c].tail = 1;
    if (runs)
        cars[c].horned = 30000;                 /* in a hurry for as long as it lasts */
    G.job_car = c;
    one_target(cars[c].x, cars[c].y);
    return true;
}

/* sets up the step at G.step; the ones that are done at once fall through */
static void story_begin(void)
{
    const mission_t *m = &missions[G.story];
    for (;;) {
        const step_t *s = &m->steps[G.step];
        float x, y, rx_, ry_;
        bool ns;
        switch (s->op) {
        case OP_END:
            story_win();
            return;
        case OP_HEAT:
            offence((float)s->n);
            G.step++;
            continue;
        case OP_TIME:
            G.timed = s->n > 0;
            G.time_wins = false;
            G.job_time = (float)s->n;
            G.step++;
            continue;
        case OP_OPEN:
            G.owns |= s->a ? OWN_BRIDGE : OWN_FERRY;
            world_open_bridge((G.owns & OWN_BRIDGE) != 0);
            message(s->a ? L("THE BRIDGE IS OPEN", "DIE BRÜCKE IST OFFEN") : L("THE FERRY RUNS AGAIN", "DIE FÄHRE FÄHRT WIEDER"), RGB(120, 255, 120), 170);
            snd_sfx(SFX_BUY, 255);
            G.step++;
            continue;
        case OP_GUN:
            pl.ammo += s->n;
            note(L("A GUN. ON FOOT, A FIRES", "WAFFE: ZU FUSS SCHIESST A"));
            G.step++;
            continue;
        case OP_GO:
        case OP_HOLD:
            place_xy(s->a, &x, &y);
            one_target(x, y);
            if (s->op == OP_HOLD) {             /* the clock runs only while the player is there */
                G.timed = G.time_wins = true;
                G.job_time = (float)s->n;
            }
            break;
        case OP_STOP:
        case OP_FARE:
        case OP_BRING:
            place_xy(s->a, &x, &y);
            road_near(x, y, &rx_, &ry_);
            one_target(rx_, ry_);
            if (s->op == OP_FARE)
                fare_at_kerb(rx_, ry_);
            break;
        case OP_CAR: {
            place_xy((int)s->n, &x, &y);
            road_near(x, y, &rx_, &ry_);
            kerb(rx_, ry_, &x, &y, &ns);
            if (ns) x -= 5; else y -= 5;
            int c = car_new(s->a, s->b, x, y, ns ? 0 : PI / 2, AI_NONE);
            for (int i = 0; c < 0 && i < CARS; i++)     /* no room: something out of sight goes */
                if (cars[i].used && cars[i].ai != AI_PLAYER && !cars[i].keep && !on_screen(cars[i].x, cars[i].y, 20)) {
                    car_remove(i);
                    c = car_new(s->a, s->b, x, y, ns ? 0 : PI / 2, AI_NONE);
                }
            if (c < 0) {
                job_end(false, L("TRY AGAIN LATER", "SPÄTER NOCHMAL"));
                return;
            }
            cars[c].keep = 1;
            G.job_car = c;
            one_target(x, y);
            break;
        }
        case OP_KILL:
        case OP_TAIL:
            if (!story_car(s->op == OP_KILL ? s->a : CAR_SEDAN, s->op == OP_KILL)) {
                job_end(false, L("TRY AGAIN LATER", "SPÄTER NOCHMAL"));
                return;
            }
            G.spot = 0;
            G.lost = 0;
            break;
        case OP_DROPS:
            place_xy(s->a, &x, &y);
            targets_clear();
            for (int i = 0; i < s->n && i < TARGETS; i++) {
                road_point(x, y, 140 + 80.0f * (float)i, 240 + 100.0f * (float)i, &rx_, &ry_);
                G.tgt[i].x = rx_;
                G.tgt[i].y = ry_;
                G.tgt[i].on = true;
            }
            G.left = s->n;
            break;
        default:                                /* OP_LOSE */
            targets_clear();
            break;
        }
        snprintf(G.job_text, sizeof G.job_text, "%s", s->text[rt_lang] ? s->text[rt_lang] : "");
        G.sub = 1;
        return;
    }
}

static void story_next(void)
{
    snd_sfx(SFX_COIN, 255);
    G.step++;
    G.sub = 0;
    story_begin();
}

/* once a tick; true while a mission runs, so that the phones' jobs wait */
static bool story_step(void)
{
    if (G.mode != MODE_STORY)
        return false;
    if (G.story_wait > 0)
        G.story_wait--;
    if (G.job == J_NONE) {
        giver_t g;
        if (pl.car < 0 && !G.story_wait && story_giver(&g) && dist2(pl.x, pl.y, g.x, g.y) < 15 * 15) {
            G.talk = G.story;
            G.state = ST_TALK;
            G.card = RT_ART && G.card_seen != G.story + 1 ? card_before(G.story) : -1;
            G.card_seen = G.story + 1;
            snd_sfx(SFX_UI, 255);
        }
        return false;
    }
    if (G.job != J_STORY)
        return false;
    if (G.timed && !G.time_wins) {
        G.job_time -= DT;
        if (G.job_time <= 0) {
            job_end(false, L("OUT OF TIME", "ZEIT ABGELAUFEN"));
            return true;
        }
    }
    const step_t *s = &missions[G.story].steps[G.step];
    car_t *jc = G.job_car >= 0 ? &cars[G.job_car] : NULL;
    bool driving = pl.car >= 0;
    float speed = pspeed(), d;
    int hit = target_hit();

    switch (s->op) {
    case OP_GO:
        if (hit >= 0)
            story_next();
        break;
    case OP_STOP:
    case OP_FARE:
        if (hit < 0 || !driving || speed > 30)
            break;
        if (s->op == OP_FARE) {
            for (int i = 0; i < PEDS; i++)
                if (peds[i].used && peds[i].state == P_WAIT)
                    peds[i].used = 0;
            snd_sfx(SFX_DOOR, 255);
        }
        story_next();
        break;
    case OP_CAR:
        if (!jc || !jc->used || jc->hp <= 0) {
            job_end(false, L("IT IS A WRECK", "NUR NOCH SCHROTT"));
            break;
        }
        G.tgt[0].x = jc->x;
        G.tgt[0].y = jc->y;
        if (pl.car == G.job_car)
            story_next();
        break;
    case OP_BRING:
        if (!jc || !jc->used || jc->hp <= 0) {
            job_end(false, L("IT IS A WRECK", "NUR NOCH SCHROTT"));
            break;
        }
        if (pl.car == G.job_car && hit >= 0 && speed < 35) {
            int c = G.job_car;
            pl.car = -1;
            pl.x = cars[c].x;
            pl.y = cars[c].y + (float)art_car[cars[c].model].h * 0.5f + 8;
            if (!ped_ok(pl.x, pl.y))
                pl.y = cars[c].y - (float)art_car[cars[c].model].h * 0.5f - 8;
            car_remove(c);
            story_next();
        }
        break;
    case OP_LOSE:
        if (G.stars == 0)
            story_next();
        break;
    case OP_KILL:
        if (jc && jc->used && jc->hp > 0) {
            G.tgt[0].x = jc->x;
            G.tgt[0].y = jc->y;
            break;
        }
        if (jc) {                               /* wrecked, or in the water: either will do */
            jc->keep = 0;
            jc->tail = 0;
        }
        G.job_car = -1;
        story_next();
        break;
    case OP_TAIL:
        if (!jc || !jc->used || jc->hp <= 0 || jc->ai != AI_TRAFFIC) {
            job_end(false, L("HE IS GONE", "ER IST WEG"));
            break;
        }
        G.tgt[0].x = jc->x;
        G.tgt[0].y = jc->y;
        d = sqrtf(dist2(jc->x, jc->y, px(), py()));
        if (G.sub == 1) {
            if (d < 150) {
                G.sub = 2;
                G.timed = G.time_wins = true;
                G.job_time = (float)s->n;
                snprintf(G.job_text, sizeof G.job_text, L("FOLLOW, NOT TOO CLOSE", "FOLGEN, NICHT ZU DICHT"));
            }
            break;
        }
        G.job_time -= DT;
        if (d < 46)
            G.spot += 45 * DT;
        else if (G.spot > 0)
            G.spot -= 15 * DT;
        if (G.spot >= 100)
            job_end(false, L("HE SAW YOU", "ER HAT DICH BEMERKT"));
        else if (d > 240 && ++G.lost > 150)
            job_end(false, L("YOU LOST HIM", "DU HAST IHN VERLOREN"));
        else if (G.job_time <= 0) {
            jc->keep = 0;
            jc->tail = 0;
            G.job_car = -1;
            G.timed = G.time_wins = false;
            story_next();
        }
        if (d <= 240)
            G.lost = 0;
        break;
    case OP_DROPS:
        if (hit < 0)
            break;
        G.tgt[hit].on = false;
        if (--G.left == 0)
            story_next();
        else
            snd_sfx(SFX_COIN, 255);
        break;
    case OP_HOLD:
        if (dist2(px(), py(), G.tgt[0].x, G.tgt[0].y) < 70 * 70)
            G.job_time -= DT;
        if (G.job_time <= 0) {
            G.timed = G.time_wins = false;
            story_next();
        }
        break;
    default:
        break;
    }
    return true;
}

/* the briefing: A takes the mission, B leaves it for later */
static void talk_step(void)
{
    if (G.card >= 0) {                          /* the picture first; any button goes on */
        if (PRESSED(BTN_A | BTN_B | BTN_C)) {
            G.card = -1;
            snd_sfx(SFX_UI, 255);
            if (G.talk < 0)
                G.state = ST_PLAY;
        }
        return;
    }
    if (PRESSED(BTN_B | BTN_C)) {
        G.state = ST_PLAY;
        G.story_wait = 150;
        return;
    }
    if (!PRESSED(BTN_A))
        return;
    G.state = ST_PLAY;
    G.job = J_STORY;
    G.step = 0;
    G.sub = 0;
    G.job_car = -1;
    G.reward = 0;
    G.spot = 0;
    G.lost = 0;
    message(missions[G.story].title[rt_lang], RGB(255, 220, 64), 130);
    snd_sfx(SFX_JOB, 255);
    story_begin();
}

#ifdef RT_HOST
/* tools/host.c: do what the step asks for, by magic. Called every tick of a
 * test run; nothing of this is in the game on the device. */
void story_debug_solve(void)
{
    if (G.job != J_STORY)
        return;
    const step_t *s = &missions[G.story].steps[G.step];
    car_t *jc = G.job_car >= 0 ? &cars[G.job_car] : NULL;
    float x = px(), y = py();
    game_target(&x, &y);
    if (s->op != OP_LOSE || G.frame % 50 == 0) {    /* the police look away */
        G.heat = 0;
        G.stars = 0;
    }
    switch (s->op) {
    case OP_CAR:
        if (jc && pl.car < 0) {
            jc->ai = AI_PLAYER;
            pl.car = G.job_car;
        }
        break;
    case OP_STOP:
    case OP_FARE:
        for (int k = 0; k < CARS && pl.car < 0; k++)
            if (cars[k].used && cars[k].ai == AI_NONE && cars[k].hp > 0) {
                cars[k].ai = AI_PLAYER;
                pl.car = k;
            }
        if (pl.car < 0)
            pl.car = car_new(CAR_SEDAN, 0, x, y, 0, AI_PLAYER);
        /* fall through */
    case OP_GO:
    case OP_BRING:
    case OP_DROPS:
    case OP_HOLD:
        if (pl.car >= 0) {
            cars[pl.car].x = x;
            cars[pl.car].y = y;
            cars[pl.car].vx = cars[pl.car].vy = 0;
        }
        pl.x = x;
        pl.y = y;
        break;
    case OP_KILL:
        if (jc)
            jc->hp = 0;
        break;
    case OP_TAIL:
        if (pl.car >= 0) {
            cars[pl.car].ai = AI_NONE;
            pl.car = -1;
        }
        if (jc) {                                   /* a little behind, on anything that can be stood on */
            pl.x = jc->x - sinf(jc->a) * 90;
            pl.y = jc->y + cosf(jc->a) * 90;
            pl.inv = 50;
        }
        break;
    default:
        break;
    }
}
#endif
