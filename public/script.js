const grid = document.getElementById("periodicGrid");
const statusText = document.getElementById("status");
const count = document.getElementById("count");
const searchForm = document.getElementById("searchForm");
const searchInput = document.getElementById("searchInput");
const filterInput = document.getElementById("filterInput");
const categoryFilter = document.getElementById("categoryFilter");
const sortSelect = document.getElementById("sortSelect");

let elements = [];

async function loadElements() {
  try {
    const response = await fetch("/api/elements");

    if (!response.ok) {
      throw new Error("C server returned an error");
    }

    elements = await response.json();

    if (!Array.isArray(elements) || elements.length !== 118) {
      throw new Error("Expected 118 element records");
    }

    statusText.textContent = "Connected to C backend successfully.";
    statusText.style.color = "#70f0ca";

    populateCategories();
    renderElements();
  } catch (error) {
    statusText.textContent =
      "Cannot connect to the C backend. Start the server and refresh.";
    statusText.style.color = "#ff8a8a";
    console.error(error);
  }
}

function populateCategories() {
  const categories = [...new Set(
    elements.map(element => element.category)
  )].sort();

  categories.forEach(category => {
    const option = document.createElement("option");
    option.value = category;
    option.textContent = category;
    categoryFilter.appendChild(option);
  });
}

function renderElements() {
  const filter = filterInput.value.trim().toLowerCase();
  const selectedCategory = categoryFilter.value;

  let visible = elements.filter(element => {
    const matchesText = [
      element.name,
      element.symbol,
      String(element.atomicNumber),
      element.category
    ].some(value => value.toLowerCase().includes(filter));

    const matchesCategory =
      !selectedCategory || element.category === selectedCategory;

    return matchesText && matchesCategory;
  });

  if (sortSelect.value === "mass") {
    visible.sort((a, b) => a.atomicMass - b.atomicMass);
  } else if (sortSelect.value === "name") {
    visible.sort((a, b) => a.name.localeCompare(b.name));
  } else {
    visible.sort((a, b) => a.atomicNumber - b.atomicNumber);
  }

  grid.replaceChildren();

  visible.forEach(element => {
    const card = document.createElement("button");
    card.type = "button";
    card.className = "element";

    const number = document.createElement("span");
    number.className = "number";
    number.textContent = element.atomicNumber;

    const symbol = document.createElement("span");
    symbol.className = "symbol";
    symbol.textContent = element.symbol;

    const name = document.createElement("span");
    name.className = "name";
    name.textContent = element.name;

    const mass = document.createElement("span");
    mass.className = "mass";
    mass.textContent = Number(element.atomicMass).toFixed(3);

    card.append(number, symbol, name, mass);
    card.addEventListener("click", () => showDetails(element));
    grid.appendChild(card);
  });

  count.textContent = `${visible.length} of ${elements.length} elements`;
  document.getElementById("emptyMessage")
    .classList.toggle("hidden", visible.length !== 0);
}

function showDetails(element) {
  document.getElementById("detailSymbol").textContent = element.symbol;
  document.getElementById("detailName").textContent = element.name;
  document.getElementById("detailMeta").textContent =
    `${element.name} (${element.symbol})`;
  document.getElementById("detailNumber").textContent =
    element.atomicNumber;
  document.getElementById("detailMass").textContent =
    `${Number(element.atomicMass).toFixed(3)} u`;
  document.getElementById("detailCategory").textContent =
    element.category;

  document.getElementById("detail").classList.remove("hidden");
  document.getElementById("detail").scrollIntoView({
    behavior: "smooth",
    block: "nearest"
  });
}

searchForm.addEventListener("submit", async event => {
  event.preventDefault();

  const query = searchInput.value.trim();
  if (!query) return;

  try {
    statusText.textContent = "Searching in C backend...";

    const response = await fetch(
      `/api/search?q=${encodeURIComponent(query)}`
    );

    const result = await response.json();

    if (!response.ok) {
      statusText.textContent = result.error || "Element not found.";
      statusText.style.color = "#ff8a8a";
      return;
    }

    statusText.textContent = "Element found by C backend.";
    statusText.style.color = "#70f0ca";
    showDetails(result);
  } catch (error) {
    statusText.textContent = "Search failed. Check the C server.";
    statusText.style.color = "#ff8a8a";
    console.error(error);
  }
});

filterInput.addEventListener("input", renderElements);
categoryFilter.addEventListener("change", renderElements);
sortSelect.addEventListener("change", renderElements);

document.getElementById("closeDetail").addEventListener("click", () => {
  document.getElementById("detail").classList.add("hidden");
});

loadElements();

