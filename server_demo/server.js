const files_list = document.getElementsByTagName("ul")[0],
  current_url = window.location.href,
  current_path = new URL(current_url).pathname,
  file_preview = document.getElementById("file-preview");

// This is the 'li' element which would be selected
var selected_file = null;

function show_file() {
  if (selected_file == null)
    return;
  if (selected_file.textContent.endsWith("/"))
    return;

  fetch(current_path + "/" + selected_file.textContent)
    .then(async response => {
      file_preview.innerHTML = await response.text();
    });
};

function select_file(to_select) {
  if (to_select == null)
    return;

  if (selected_file)
    selected_file.classList.remove("highlight");
  to_select.classList.add("highlight");
  selected_file = to_select;

  show_file();
}

// Hanldes motions and keypresses to change the highlighted file
function handle_motion() {
  document.onkeyup = (event) => {
    if (event.key == "j" || event.key == "Down" || event.key == "ArrowDown")
      select_file(selected_file.nextElementSibling);
    if (event.key == "k" || event.key == "Up" || event.key == "ArrowUp")
      select_file(selected_file.previousElementSibling);
    if (event.key == "G")
      select_file(files_list.lastElementChild);
    if (event.key == "g")
      select_file(files_list.firstElementChild);
    if (event.key == "Enter") {
      var file_name = selected_file.textContent;
      if (file_name.endsWith("/"))
        window.location.href = current_url + "/" + file_name.substring(0, file_name.length - 1);
      else
        window.location.href = current_url + "/" + file_name;
    }
  }
}

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById("current_url").textContent = current_path;

  for (const file of files_list.children) {
    file.onclick = () => {
      window.location.href = current_url + '/' + file.textContent;
    };
  }

  // Highlighting the first file
  select_file(files_list.firstElementChild);

  handle_motion();
});
