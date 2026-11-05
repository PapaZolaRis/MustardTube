# v0.9 device acceptance

Only muOS 2606.0 Andromeda on RG35XX Plus / SP is in scope.
No older firmware compatibility work is included.

1. Search a broad topic. Press X at least four times. Confirm additional unique
   results appear, selection moves to the new batch, and the footer only hides X
   at the actual end or the 40-video limit. Check a narrow search too.
2. Disconnect Wi-Fi for Load More, reconnect and retry. Existing results survive.
3. Pause on a result, then open Details. Check the thumbnail and A Play behavior.
   Move quickly between results and cancel with B; old preparation must not delay
   the next selection or leave extraction processes running.
4. Switch Y between 360p and 480p. Confirm audio/video, readable quality label,
   persistence after restart and no resolution above the selected limit.
5. MENU + START exits stock Wasabi and returns to Details with title/channel.
   B returns to the selected result. Check return after an unavailable video too.
6. Style → X Clear cache: see confirmation; theme/quality/saved preferences stay.
   A subsequent search and video play must work without reinstalling anything.
7. Check RG35XX SP lid/suspend/resume and both models' actual controller mappings.
8. Compare the same 10 videos and Wi-Fi against v0.8. Record fresh and prepared
   runs separately. Time button press to first visible frame externally; report
   median, slowest cases, failures and rebuffering. Do not infer first-frame timing
   from player process launch or session length.

The first-frame API and supported Wasabi buffering flags were not verified, so
v0.9 does not alter stock player options or replace the player.
