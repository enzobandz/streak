# Storyboard — *What If the Entire Internet Shut Down for 24 Hours?*

1920×1080 · 30 fps · 7:58 · every shot is a real-time Three.js scene rendered frame-by-frame by Remotion.
Shot boundaries and on-screen events are keyed to narration lines and to the natural pauses detected in
each narration clip (`useLines()` → `L.<lineId>.from / .end / .p[n]`), so re-recording the voice re-times the picture.

**Recurring visual language**

| Element | Meaning |
|---|---|
| Cyan `#3FE0FF` pulses / glow | live data, connectivity |
| Coral `#FF4D6D` | offline, failure |
| Green `#5CF2A0` + ✓ badge | still working |
| Amber `#FFB547` | human warmth, caution, key takeaway |
| Top-right ring clock "OFFLINE FOR hh:00" | progress through the 24 hours (animates between chapters) |
| Bottom-left "SOURCE" tag | appears automatically for any line that makes a numerical/historical claim |
| Diagonal light-wipe + whoosh | chapter transition |

| Time | Scene (component) | Shot | Picture | Key sync points | Sound |
|---|---|---|---|---|---|
| 0:00 | **Cold open** `ColdOpen.tsx` | 1 continuous move | Dotted Earth (Natural Earth land mask) in space; illustrative submarine-cable arcs draw on; data pulses race. "GLOBAL NETWORK · LIVE" HUD with sparkline. Camera orbits from the Atlantic toward Asia. | c2 → "5.5 billion" stat; Money/Medicine/Maps/Messages cards pop on each word. c3 → UTC countdown 23:59:57… hits **00:00:00 on "stopped"**: cables turn red, pulses die, glitch. "FOR 24 HOURS". | Riser, pops, three clock ticks, power-down, glitch, sub boom. Score cuts to silence. |
| 0:25 | **Title** `Title.tsx` | 1 | Dark (offline) globe from below; channel logo; title words spring in, "24 Hours?" in coral. | Frame 0 hit | Cinematic hit + shimmer |
| 0:30 | **The Network of Networks** `Network.tsx` | A | Studio void, giant light switch; it jiggles, then **NO OFF SWITCH** stamp. | n1 second pause | Switch clicks, hit |
| | | B | 3D graph of 81 networks in 9 clusters, colour-coded by operator type; "70,000+" counter; legend rows pop on "phone companies / universities / cloud providers / governments". Central **Internet Exchange** hub appears; an amber **BGP route** lights hop-by-hop. | n2 pauses; n3 start; n3 pauses | Pops, whoosh, blip, riser |
| | | C | Underwater tracking shot along a cable on the seabed (marine snow, rocks, light pulses). "500+" stat, then layered **cable cross-section ≈ a garden hose**. | n4 pauses | Whoosh, pop |
| | | D | Pull back from the live globe; **THOUGHT EXPERIMENT** stamp, "grounded in real outages". | n5 first pause | Hit |
| 1:21 | **Hour 0 — The First Minutes** `FirstMinutes.tsx` | A | Split screen: Tokyo 09:00 morning skyline / New York 19:00 EST evening skyline. | f1 pause 1 | Score |
| | | B | Network graph: global routes withdrawn, clusters drift apart into islands. | f1 pause 2 | Glitch, power-down |
| | | C | Street: people walking with phones slow to a stop and look worried; billboard video freezes; "Not delivered", "Sending…", spinner bubbles. | f2 pauses | City ambience, error buzzes |
| | | D | Night: GPS satellites keep beaming to a person's phone; phone UI shows **GPS OK** but grey map tiles → "Offline map saved". | f3 pauses | Radar ping, chime |
| | | E | Living room: character presses the router button; LEDs blink amber. Cut to an apartment block where every window's router blinks in unison. "…it won't help this time." | f4 pause 2 | Switch, blip, ticks |
| 2:05 | **Hour 1 — Money Stops Moving** `Money.tsx` | A | Coins and cards rain down, then freeze mid-air (desaturate). | end of m1 | Coins, glitch, boom |
| | | B | Café counter: card terminal Connecting → amber stored-offline → red limit; barista shrugs. Status panel explains bank authorisation and offline limits. | m2 pauses | Blip, error, chime |
| | | C | Online banking / wallets / instant transfers cards flip ✗; ATM screen turns red "Out of service"; **CASH IS KING** — character holds cash. | m3 pauses | Errors, power-down, coin |
| | | D | Trading board with live bars and scrolling ticker freezes grey → **TRADING HALTED**. | m4 pauses | Typing, hit |
| | | E | 365 slabs = one year of world output; "$110 trillion" → one slab glows: "≈ $300 billion every day" → "Not all would stop…". | m5 pauses | Whoosh, blip |
| 3:00 | **Hour 3 — Stuck on the Ground** `Transport.tsx` | A | Airliner above low-poly clouds; camera drops to the control tower; radio rings + radar sweep; "Radio ✓ Radar ✓". | t1 pauses | Jet, static, radar |
| | | B | Gate with parked plane and service truck; Bookings / Check-in / Crew schedules / Load planning cards appear then fail. | t2 pauses | Pops, error |
| | | C | Office floor: 48 monitors turn blue in a wave (July 2024 CrowdStrike update). "≈ 8.5 million devices", "Thousands of flights canceled", "…not even an internet outage". | t3 pauses | Glitch, errors, hit |
| | | D | Split-flap departure board flips to CANCELED/DELAYED; a plane still lands at sunset. | t4 pause | Flap ticks, jet |
| | | E | Container port: ship, cranes, stacks; trucks queue at a closed gate. "Cargo: fine / Paperwork: jammed"; Containers / Customs / Trucks warnings. | t5 pauses | Ambience, pops |
| 3:55 | **Hour 6 — What Keeps Working** `Critical.tsx` | A | Golden hour: power station → pylons with green energy pulses → town with lights on. | k2 pauses | Hum, chime |
| | | B | Water-treatment tanks with rotating arms; operator with clipboard. | k2b pauses | Pops |
| | | C | Hospital exterior; nurse with clipboard, runner carrying papers; Paper charts / Printed med lists / Results by hand ✓. | k3 pauses | Pops |
| | | D | Night town with cell masts; call bubbles "Calling Mom…", "Network busy". | k4 pauses | Pings, errors |
| | | E | Broadcast mast emitting green rings; a family gathered round a radio. | k5 pauses | Static, chime |
| 4:57 | **Hour 9 — The Office Goes Quiet** `Work.tsx` | A | European skyline at sunrise; "09:00 UTC"; "Modern work lives in the cloud". | w1 pauses | Whoosh |
| | | B | Open-plan office: Email / Documents / Video calls / Payroll / Stock & orders fail one by one; staff shrug and leave. | w2 pauses | Typing, errors |
| | | C | Factory: robot arms keep working, conveyor boxes pile up; New orders / New parts / Shipping info warnings. | w3 pauses | Hum, pops |
| 5:30 | **Hour 12 — The Human Side** `Human.tsx` | A | Globe turns to Egypt; pulsing marker on Cairo; **2011 · Egypt · about five days**. | h2 start | Low pulse |
| | | B | Pacific island volcano erupts (ash plume), the island's only cable glows red and breaks; **2022 · Tonga · about five weeks**. | h3 pauses | Boom, glitch |
| | | C | Grid of 296 cells fills while a counter climbs; "documented in 2024 · 54 countries · Access Now". | h4 pauses | Ticks |
| | | D | Neighbourhood street: costs (Lost income / Families cut off / Businesses frozen) then the light warms and people adapt (Neighbors share news / Debts in notebooks / Life gets more local). | h5 pauses | Ambience, chime |
| 6:22 | **Hour 24 — Switching It Back On** `Restart.tsx` | A | The switch from chapter 1 flips back ON (callback); "…but not like flicking on a light". | r1 pauses | Switch, power-up |
| | | B | Network islands drift back together, links reconnect piece by piece. | r2 pauses | Blips |
| | | C | 1,400 request particles converge on server racks; illustrative load chart spikes; **Thundering Herd**. | r3 pauses | Riser, boom |
| | | D | Data-centre hall; power gauge swings to "surge risk"; racks then power up section by section. "Facebook, Oct 2021". | r4 pauses | Hum, error, switches |
| | | E | Globe re-lighting; backlog bars clearing slowly; "Hours… maybe days". | r5 | Score |
| 7:13 | **What We'd Learn** `Outro.tsx` | A | Dawn skyline, plane on approach; Water flows / Lights stay on / Planes land ✓. | o1 pauses | Chimes |
| | | B | Live globe: "Invisible agreements between machines" → "How many of us have a plan B?" | o2 pause | Score |
| | | C | Two neighbours wave; **Your plan B** checklist ticks off cash, offline map, numbers on paper, neighbours. | o3 pauses | Pops |
| | | D | Globe pulls back: "The internet is remarkably resilient." → **Are we?** | o4 pause | Hit |
| | | E | End card: logo, "Thanks for watching", Subscribe → Subscribed, "What should we imagine next?" | o5 pauses | Pop, click; music resolves and fades |
