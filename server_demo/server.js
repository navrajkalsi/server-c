const files_list = document.querySelector("ul"), // Gets the first ul element
  current_url = window.location.href,
  current_path = new URL(current_url).pathname,
  file_preview = document.getElementById("file-preview").firstElementChild,
  response_image = document.getElementById("response-image");

// This is the 'li' element which would be selected
var selected_file = null;

// Takes in a relative url/path and make a request for the current_url + url
function visit_url(url) {
  if (!url)
    return;
  if (url.endsWith("/"))
    url = url.slice(0, -1);

  window.location.href = current_url + "/" + url;
}

// Makes a request for the selected file name, if any. Fills the file preview element with the response
function show_file() {
  if (selected_file == null)
    return;

  // Handling user selecting a directory
  if (selected_file.textContent.endsWith("/")) {
    fetch(current_path + "/" + selected_file.textContent)
      .then(async response => {
        // Using innerHTML instead of textContent cause I want to render the ul element in the DOM and not just copy and paste the response in pre tag
        var response_text = await response.text();
        file_preview.innerHTML = response_text.substring(response_text.indexOf("<ul"), response_text.indexOf("</ul>") + 5);
      });
  }
  else
    fetch(current_path + "/" + selected_file.textContent)
      .then(async response => {
        const contentType = response.headers.get("Content-Type");

        // If image then fill the blob
        if (contentType && contentType.startsWith("image/"))
          return response.blob();
        else {
          // Clearing image
          response_image.src = "";
          file_preview.textContent = await response.text();
          return null;
          // if not iamge then add text to pre and exit
        }
      })
      .then(imageBlob => {
        // imageBlob will not be null, if we returned response.blob()
        if (imageBlob) {
          file_preview.textContent = "";
          response_image.src = URL.createObjectURL(imageBlob);
        }
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
      visit_url(file_name);
    }
  }
}

document.addEventListener("DOMContentLoaded", () => {
  // Just so further requests do not contain double /
  // Won't cause any errors if it does but looks weird
  if (current_path.endsWith("/"))
    window.location.href = current_url.slice(0, -1);

  document.getElementById("current_url").textContent = current_path;

  files_list.id = "main-list"; // Adding id to the list for styles

  for (const file of files_list.children) {
    file.onclick = () =>
      visit_url(file.textContent);
  }

  // Highlighting the first file
  select_file(files_list.firstElementChild);

  handle_motion();
});
