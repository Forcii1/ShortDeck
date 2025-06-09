function navigateWithFade(targetUrl, fadeType = "true") {
  const fade = document.getElementById("page-fade");

  fade.classList.remove("fade-in", "hidden");
  void fade.offsetWidth; // Reflow erzwingen

  fade.classList.add("fade-out");

  const duration = (fadeType === "slow") ? 500 : 100;

  setTimeout(() => {
    sessionStorage.setItem("fadeInNextPage", fadeType);
    window.location.href = targetUrl;
  }, duration);
}

window.addEventListener("DOMContentLoaded", () => {
  const fade = document.getElementById("page-fade");
  const fadeType = sessionStorage.getItem("fadeInNextPage");
  sessionStorage.removeItem("fadeInNextPage");

  if (fadeType === "true") {
    fade.classList.remove("hidden", "fade-out");
    fade.classList.add("fade-in");

    fade.addEventListener("animationend", () => {
      fade.classList.remove("fade-in");
      fade.classList.add("hidden");
    }, { once: true });

  } else if (fadeType === "slow") {
    fade.classList.remove("hidden", "fade-out");
    fade.classList.add("fade-in-slow");

    fade.addEventListener("animationend", () => {
      fade.classList.remove("fade-in-slow");
      fade.classList.add("hidden");
    }, { once: true });

  } else {
    fade.classList.add("hidden");
  }
});
