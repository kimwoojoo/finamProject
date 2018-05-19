<script language='javascript'>
  window.setTimeout('window.location.reload()',5000);
</script>

<?php
$connect = mysqli_connect("115.68.228.55", "root", "wownsdnwnalstj")
   or die("Connect Fail: " . mysqli_error());
echo "Connect Success!";
echo "<h1> WooJoo View DB </h1>";
echo "Server Host : 115.68.228.55";

mysqli_select_db($connect,"demofarmdb") or die("Select DB Fail!");

$query = "select * from thl order by time desc limit 10";
$result = mysqli_query($connect, $query) or die("Query Fail: " . mysqli_error());

echo "<table>\n";

echo "<tr><td>Time</td><td>TEMP</td><td>LIGHT</td>";

while ($line = mysqli_fetch_assoc($result)) {
   echo "\t<tr>\n";
   foreach ($line as $col_value) {
       echo "\t\t<td>$col_value</td>\n";
   }
   echo "\t</tr>\n";
}

echo "</table>\n";

mysqli_free_result($result);

mysqli_close($connect);
?>
