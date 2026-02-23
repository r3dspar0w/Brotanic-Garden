const questions = [
  {
    question: "Why do plants need water?",
    answers: ["To make noise", "To grow and stay healthy", "To change color", "To stay happy"],
    correct: 1,
  },
  {
    question: "What happens if a plant gets too little water?",
    answers: ["It grows very fast", "It wilts and dries", "It becomes stronger", "Nothing happens"],
    correct: 1,
  },
  {
    question: "What sensor is commonly used to detect soil water level?",
    answers: ["Temperature sensor", "Soil moisture sensor", "Light sensor (LDR)", "Pressure sensor"],
    correct: 1,
  },
  {
    question: "What is the MAIN goal of a smart garden system?",
    answers: ["Automate plant care efficiently", "Make plants talk", "Increase soil weight", "Change leaf color automatically"],
    correct: 0,
  },
  {
    question: "Which condition helps flowering plants bloom best?",
    answers: ["Proper sunlight + water + nutrients", "No water at all", "Complete darkness", "Only cold temperature"],
    correct: 0,
  },
];

const quizPanel = document.getElementById("quizPanel");
const resultPanel = document.getElementById("resultPanel");
const progressBar = document.getElementById("progressBar");
const questionCount = document.getElementById("questionCount");
const questionText = document.getElementById("questionText");
const answersWrap = document.getElementById("answers");
const scoreLine = document.getElementById("scoreLine");
const voucherLine = document.getElementById("voucherLine");
const countdown = document.getElementById("countdown");
const gameMusic = document.getElementById("gameMusic");
const resultTitle = resultPanel.querySelector("h2");
const resultSubtitle = resultPanel.querySelector(".subtitle");
const resultConfetti = resultPanel.querySelector(".confetti");

let index = 0;
let score = 0;

function startGameMusic() {
  if (!gameMusic) {
    return;
  }

  gameMusic.volume = 0.55;
  const playPromise = gameMusic.play();
  if (playPromise && typeof playPromise.catch === "function") {
    playPromise.catch(() => {
      const retryPlay = () => {
        gameMusic.play().catch(() => {});
        document.removeEventListener("pointerdown", retryPlay);
        document.removeEventListener("keydown", retryPlay);
      };

      document.addEventListener("pointerdown", retryPlay, { once: true });
      document.addEventListener("keydown", retryPlay, { once: true });
    });
  }
}

function renderQuestion() {
  const q = questions[index];
  questionCount.textContent = `Question ${index + 1} / ${questions.length}`;
  questionText.textContent = q.question;
  progressBar.style.width = `${(index / questions.length) * 100}%`;
  answersWrap.innerHTML = "";

  q.answers.forEach((answer, answerIndex) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "answer-btn";
    button.textContent = answer;
    button.addEventListener("click", () => selectAnswer(button, answerIndex));
    answersWrap.appendChild(button);
  });
}

function selectAnswer(button, answerIndex) {
  const isCorrect = answerIndex === questions[index].correct;
  if (isCorrect) {
    score += 1;
    button.classList.add("correct");
  } else {
    button.classList.add("wrong");
  }

  answersWrap.querySelectorAll("button").forEach((btn) => {
    btn.disabled = true;
  });

  setTimeout(() => {
    index += 1;
    if (index < questions.length) {
      renderQuestion();
    } else {
      showResult();
    }
  }, 550);
}

function showResult() {
  progressBar.style.width = "100%";
  quizPanel.classList.add("hidden");
  resultPanel.classList.remove("hidden");
  const passed = score > 3;

  if (resultTitle) {
    resultTitle.textContent = passed ? "Congratulations!" : "Try Again";
  }
  if (resultSubtitle) {
    resultSubtitle.textContent = passed
      ? "You finished the Plant Care Quiz"
      : "Keep learning plant care and try one more time.";
  }
  if (resultConfetti) {
    resultConfetti.classList.toggle("hidden", !passed);
  }

  scoreLine.textContent = `Score: ${score} / ${questions.length}`;

  let voucher = "$0";
  if (score >= 5) {
    voucher = "$5";
  } else if (score >= 4) {
    voucher = "$3";
  } else if (score >= 3) {
    voucher = "$2";
  } else if (score >= 2) {
    voucher = "$1";
  }

  voucherLine.textContent = `Voucher Value: ${voucher}`;

  let seconds = 6;
  countdown.textContent = String(seconds);
  const timer = setInterval(() => {
    seconds -= 1;
    countdown.textContent = String(seconds);
    if (seconds <= 0) {
      clearInterval(timer);
      window.location.href = "index.html";
    }
  }, 1000);
}

renderQuestion();
startGameMusic();

